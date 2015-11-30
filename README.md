How to recreate this repository
-------------------------------

The original repository contains "submodules", which Yocto / bitbake
support. The original repository also uses autotools, which Yocto /
bitbake also support. The problem appears when trying to use both things
at the same time: the existing Yocto / bitbake support does not have the
concept of configure scripts in _multiple_ subdirectories. The way
autotools supports this is by having a statement like this:

	AC_CONFIG_SUBDIRS([submodules/bm])

in the toplevel `configure.ac`. This is the reason why this repository
added that file, to be able to drive the configure process of the
submodules.

Since trying to manage the submodules proved to be a pain in the context
of Yocto, the git submodules were replaced with git subtrees, which
creates a single repository with the contents of the submodules added to
it. In order to recreate the repository, do this:

	$ git remote add -f behavioral-model https://github.com/p4lang/behavioral-model.git
	$ git subtree add --prefix submodules/bm behavioral-model master --squash

	$ git remote add -f submodules/p4c-bm https://github.com/p4lang/p4c-bm.git
	$ git subtree add --prefix submodules/p4c-bm p4c-bm master --squash

If you need to update a subtree, do this:

	$ git subtree pull --prefix submodules/bm behavioral-model master --squash

Instead of master, you can use any valid reference (commit, branch,
tag). Just keep in mind that it refers to the remote you are specifying
and not to local references.

Strictly speaking it's not necessary to add the remotes and you can use
the URL in place of the remote name in the corresponding 'git subtree add' line.

`git subtree` is mostly syntax sugar for adding git trees at specific
graft points. This [blog post from
Atlassian](http://blogs.atlassian.com/2013/05/alternatives-to-git-submodule-git-subtree/)
contains a nice summary.
