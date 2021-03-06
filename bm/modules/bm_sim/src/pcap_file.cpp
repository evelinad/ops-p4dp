/* Copyright 2013-present Barefoot Networks, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "bm_sim/pcap_file.h"

#include <stdexcept>
#include <cassert>
#include <chrono>
#include <thread>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

#include <cstring>

#define DEBUG_TIMING 0

namespace bm {

namespace {

// TODO(antonin): remove
void bm_fatal_error(std::string message) {
  std::cerr << "FATAL: " << message << std::endl;
  exit(1);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

// This method should be in some separate C library
// Converts a timeval into a string.
// static
std::string
PcapPacket::timevalToString(const struct timeval* tv) {
  std::stringstream result;
  result << std::to_string(tv->tv_sec) << ":" <<
    std::setfill('0') << std::setw(6) <<
    std::to_string(tv->tv_usec);
  return result.str();
}

////////////////////////////////////////////////////////////////////////////////

PcapFileBase::PcapFileBase(unsigned port, std::string filename)
  : port(port),
    filename(filename) {}

////////////////////////////////////////////////////////////////////////////////

// A stream-like abstraction of a PCAP (Packet capture) file.
PcapFileIn::PcapFileIn(unsigned port, std::string filename)
  : PcapFileBase(port, filename),
    pcap(nullptr),
    current_header(nullptr),
    current_data(nullptr) {
  reset();
}


void
PcapFileIn::deallocate() {
  if (pcap != nullptr)
    pcap_close(pcap);
  pcap = nullptr;
  current_header = nullptr;
  current_data = nullptr;
}


PcapFileIn::~PcapFileIn() {
  deallocate();
}


void
PcapFileIn::open() {
  char errbuf[PCAP_ERRBUF_SIZE];

  pcap = pcap_open_offline(filename.c_str(), errbuf);
  if (pcap == nullptr)
    bm_fatal_error(errbuf);
  state = State::Opened;
}

void
PcapFileIn::reset() {
  deallocate();
  state = State::Uninitialized;
  open();
}

bool
PcapFileIn::moveNext() {
  switch (state) {
  case State::AtEnd:
    return false;
  case State::Opened:
  case State::Reading:
    return advance();
  case State::Uninitialized:
  default:
    bm_fatal_error("Unexpected state");
    return false;  // unreachable
  }
}

bool
PcapFileIn::advance() {
  int readbytes = pcap_next_ex(pcap, &current_header, &current_data);
  if (readbytes >= 0) {
    if (current_header->len != current_header->len)
      bm_fatal_error(
        std::string("Incompletely captured packet in ") + filename);
    state = State::Reading;
    return true;
  } else {
    state = State::AtEnd;
    return false;
  }
}

std::unique_ptr<PcapPacket>
PcapFileIn::current() const {
  switch (state) {
  case State::Reading:
    {
      PcapPacket* packet = new PcapPacket(port, current_data, current_header);
      return std::unique_ptr<PcapPacket>(packet);
    }
  case State::Opened:
    bm_fatal_error("Must call moveNext() before calling current()");
  case State::AtEnd:
    bm_fatal_error("Cannot read past end-of-file");
  case State::Uninitialized:
  default:
    bm_fatal_error(std::string("Unexpected state"));
  }
  return nullptr;  // unreachable
}

bool PcapFileIn::atEOF() const {
  return state == State::AtEnd;
}

////////////////////////////////////////////////////////////////////////////////

PcapFilesReader::PcapFilesReader(bool respectTiming,
                                 unsigned wait_time_in_seconds)
  : nonEmptyFiles(0),
    wait_time_in_seconds(wait_time_in_seconds),
    respectTiming(respectTiming),
    started(false) {
  timerclear(&zero);
}


void
PcapFilesReader::addFile(unsigned index, std::string file) {
  auto f = std::unique_ptr<PcapFileIn>(new PcapFileIn(index, file));
  files.push_back(std::move(f));
}


void
PcapFilesReader::scan() {
  if (nonEmptyFiles == 0) {
#if DEBUG_TIMING
    std::cout << "End of all inputs files" << std::endl;
#endif
    // notifyAllEnd();
    return;
  }

  int earliest_index = -1;
  const struct timeval* earliest_time;

  for (unsigned i=0; i < files.size(); i++) {
    auto file = files.at(i).get();
    if (file->atEOF()) continue;

    std::unique_ptr<PcapPacket> p = file->current();
    if (earliest_index == -1) {
      earliest_index = i;
      earliest_time = p->getTime();
    } else {
      const struct timeval *time = p->getTime();
      if (timercmp(time, earliest_time, <)) {
        earliest_index = i;
        earliest_time = time;
      }
    }
  }

  assert(earliest_index >= 0);
  struct timeval delay;

#if DEBUG_TIMING
  std::cout << "First packet to send "
            << PcapPacket::timevalToString(earliest_time) << std::endl;
#endif

  if (respectTiming) {
    struct timeval delta;
    timersub(earliest_time, &firstPacketTime, &delta);

    struct timeval now;
    gettimeofday(&now, nullptr);

    struct timeval elapsed;
    timersub(&now, &startTime, &elapsed);

    timersub(&delta, &elapsed, &delay);
#if DEBUG_TIMING
    std::cout << "Delta " << PcapPacket::timevalToString(&delta)
              << " elapsed " << PcapPacket::timevalToString(&elapsed)
              << " delay " << PcapPacket::timevalToString(&delay) << std::endl;
#endif
  } else {
    timerclear(&delay);
  }

  schedulePacket((unsigned)earliest_index, &delay);
}


void PcapFilesReader::timerFired() {
  auto file = files.at(scheduledIndex).get();
  std::unique_ptr<PcapPacket> packet = file->current();

  if (handler != nullptr)
    handler(packet->getPort(), packet->getData(), packet->getLength(), cookie);
  else
    bm_fatal_error("No packet handler set when sending packet");

  bool success = file->moveNext();
  if (!success)
    nonEmptyFiles--;
  scan();
}


void PcapFilesReader::schedulePacket(unsigned index, const struct timeval* at) {
  scheduledIndex = index;

  int compare = timercmp(at, &zero, <=);
  if (compare) {
    // packet should already have been sent
    timerFired();
  } else {
    auto duration = std::chrono::seconds(at->tv_sec) +
      std::chrono::microseconds(at->tv_usec);
#if DEBUG_TIMING
    std::cout << "Sleeping for " << duration.count() << std::endl;
#endif
    std::this_thread::sleep_for(duration);
    timerFired();
  }
}


void PcapFilesReader::start() {
#if DEBUG_TIMING
  std::cout << "Starting PcapFilesReader "
            << std::to_string(files.size()) << std::endl;
#endif

  // Give the switch some time to initialize
  if (wait_time_in_seconds > 0) {
#if DEBUG_TIMING
    std::cout << "Waiting for " << std::to_string(wait_time_in_seconds)
              << " seconds" << std::endl;
#endif
    auto duration = std::chrono::seconds(wait_time_in_seconds);
    std::this_thread::sleep_for(duration);
  }

  // Find out time of first packet
  const struct timeval* firstTime = nullptr;

  // Linear scan; we could use a priority queue, but probably this
  // is more efficient for a small number of files.
  for (auto &file : files) {
    bool success = file->moveNext();
    if (success) {
      nonEmptyFiles++;

      std::unique_ptr<PcapPacket> p = file->current();
      const struct timeval* ptime = p->getTime();
      if (firstTime == nullptr)
        firstTime = ptime;
      else if (timercmp(ptime, firstTime, <))
        firstTime = ptime;
    }
  }

  if (nonEmptyFiles > 0) {
    assert(firstTime != nullptr);
    firstPacketTime = *firstTime;
#if DEBUG_TIMING
    std::cout << "First packet time " << PcapPacket::timevalToString(firstTime)
              << std::endl;
#endif
  }

  if (started)
    bm_fatal_error("Reader already started");

  started = true;
  gettimeofday(&startTime, nullptr);
  scan();
}


PacketDispatcherIface::ReturnCode
PcapFilesReader::set_packet_handler(const PacketHandler &hnd, void* ck) {
  assert(hnd);
  assert(ck);

  handler = hnd;
  cookie = ck;
  return ReturnCode::SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////

PcapFileOut::PcapFileOut(unsigned port, std::string filename)
  : PcapFileBase(port, filename) {
  pcap = pcap_open_dead(0, 0);
  if (pcap == nullptr)
    bm_fatal_error(
      std::string("Could not open file ") + filename + " for writing");
  dumper = pcap_dump_open(pcap, filename.c_str());
  if (dumper == nullptr)
    bm_fatal_error(
      std::string("Could not open file ") + filename + " for writing");
}

void
PcapFileOut::writePacket(const char* data, unsigned length) {
  struct pcap_pkthdr pkt_header;
  memset(&pkt_header, 0, sizeof(pkt_header));
  gettimeofday(&pkt_header.ts, NULL);
  pkt_header.caplen = length;
  pkt_header.len = length;
  pcap_dump((unsigned char*)dumper, &pkt_header, (unsigned char*) data);
  pcap_dump_flush(dumper);
}


PcapFileOut::~PcapFileOut() {
  pcap_dump_close(dumper);
  pcap_close(pcap);
}


////////////////////////////////////////////////////////////////////////////////

PcapFilesWriter::PcapFilesWriter() {}


void
PcapFilesWriter::addFile(unsigned port, std::string file) {
  auto f = std::unique_ptr<PcapFileOut>(new PcapFileOut(port, file));
  files.emplace(port, std::move(f));
}


void
PcapFilesWriter::send_packet(int port_num, const char* buffer, int len) {
  unsigned idx = port_num;

  if (files.find(port_num) == files.end())
    // The behavior of the bmi_* library seems to be to ignore
    // packets sent to inexistent interfaces, so we replicate it here.
    return;

  auto file = files.at(idx).get();
  file->writePacket(buffer, len);
}

}  // namespace bm
