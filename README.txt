NAME
     jtransfer - securely transfer files between jails

SYNOPSIS
     jtransfer -v
     jtransfer [-r|-w] [-h] [-m mode] [-u uid[:gid]] -f file -j jid

DESCRIPTION
     jtransfer is a lightweight tool for reading a file from a FreeBSD jail
     and writing it to standard output, or for reading from standard input and
     writing the content as a file inside a FreeBSD jail. The transfer is
     implemented essentially using a standard pipe(2), following the typical
     Unix way.

     This is very different from other approaches you are likely familiar
     with:

     Transfer a file via SSH
	  This requires the jail to have an SSH implementation up and running,
	  with SFTP configured, as well as an allowed user within the jail and
	  other fun stuff related to keys or passwords, depending on how the
	  daemon is configured.  It is a suitable solution for transferring
	  files between hosts, but it is overly complex if the goal is simply
	  to transfer a file locally.

     Transfer a file by executing a command in the jail directory
	  This is the standard approach taken by most jail tools, but it
	  relies on the premise that the jail itself has not been compromised.
	  However, even the root user could be compromised; therefore, files
	  could be altered even if they are managed exclusively by the root
	  user. At a minimum, jail tools should offer an option to prevent
	  following symbolic links in case they are malicious.

	  jtransfer does not offer a magic solution for dealing with altered
	  files; however, when using cp(1), tar(1), or any other command to
	  transfer a file, you must keep in mind that you are executing them
	  from the host. Unlike those commands, jtransfer runs inside a jail;
	  always. Furthermore, unlike standard commands, there is no need to
	  deal with relative paths when writing the file, as the command
	  executes within the specific jail you specify.

     Transfer a file using sh(1) and cat(1)
	  Running `jexec -l jail sh -c cat > /path/to/file' inside a FreeBSD
	  jail is similar in style to jtransfer. However, this introduces two
	  dependencies. In most cases, this poses no problem; however, if the
	  jail is very minimal, for example, if it contains only a static
	  binary and the files it requires, this is not the right solution.

	  In reality, the internal operation of jtransfer much more closely
	  resembles that of cp(1) than that of cat(1).

     The internal operation of jtransfer is very similar to that of cp(1),
     although its usage resembles that of cat(1). This means you have a
     command that is just as efficient as the standard cp(1). However, unlike
     cp(1), jtransfer cannot use copy_file_range(2), so the file is copied
     using a more traditional approach, even if you are using ZFS as your file
     system.

     The options are as follows:

     -h   No symbolic links are followed.

     -r   Read mode.

	  The file specified with the -f option is read inside the jail and
	  written to standard output.

     -w   Write mode.

	  The content is read from standard input and written to the file
	  specified with the -f option.

     -m mode
	  File mode.

	  umask(2) is ignored. The file mode specified here will be applied to
	  the created file.

     -u uid[:gid]
	  Drop privileges after entering the jail.

     -f file
	  File path.

     -j jid
	  Perform the actions inside the jail specified by jid, which may be
	  either a jail name or a numeric jail ID.

EXAMPLES
   Read a file from the jail
	   # jtransfer -r -f /etc/rc.conf -j x11appjail-thunderbird-15000_default
	   clear_tmp_X="NO"
	   ifconfig_eb_ec6e1439cb6="inet 10.0.0.4 netmask 255.192.0.0 broadcast 10.63.255.255"
	   defaultrouter="10.0.0.1"
	   dbus_enable="YES"

   Write a file from the jail to the host
	   # jtransfer -r -f hello.txt -j x11appjail-thunderbird-15000_default > hello.txt

   Write a file from the host to the jail
	   # cat hello.txt | jtransfer -w -f hello.txt -j x11appjail-thunderbird-15000_default
	   # jtransfer -r -f hello.txt -j x11appjail-thunderbird-15000_default
	   Hello, world!

   Write a file from the jail to another jail
	   # jtransfer -r -f /etc/hosts -j x11appjail-thunderbird-15000_default |\
	       jtransfer -w -j x11appjail-telegram-desktop-15000_default -f /etc/hosts

SEE ALSO
     chmod(1) jail(3) sysexits(3)

AUTHORS
     Jesus Daniel Colmenares Oviedo <DtxdF@disroot.org>
