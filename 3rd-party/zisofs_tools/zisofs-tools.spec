Summary: Utilities to create compressed CD-ROM filesystems.
Name: zisofs-tools
Version: 1.0.7
Release: 1
License: GPL
Group: Applications/System
URL: http://www.kernel.org/pub/linux/utils/fs/zisofs/
Source: http://www.kernel.org/pub/linux/utils/fs/zisofs/zisofs-tools-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root
Epoch: 1

%description
Tools that, in combination with an appropriately patched version of
mkisofs, allow the creation of compressed CD-ROM filesystems.

%prep
%setup -q 

%build
%configure
make

%install
rm -rf $RPM_BUILD_ROOT
make install INSTALLROOT="$RPM_BUILD_ROOT"

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc README zisofs.magic
%{_bindir}/mkzftree
%{_mandir}/man1/mkzftree.1*

%changelog
* Mon Jul 20 2004 H. Peter Anvin <hpa@zytor.com> 1.0.6-1
- Generalize and include in 1.0.6 so rpmbuild -ta works.

* Sat Jun 13 2004 H. Peter Anvin <hpa@zytor.com> 1.0.5-1
- Revision update.

* Wed Nov  6 2002 H. Peter Anvin <hpa@zytor.com> 1.0.4-1
- Revision update.

* Thu Nov  8 2001 H. Peter Anvin <hpa@zytor.com> 1.0.3-1
- Revision update.

* Mon Oct 29 2001 H. Peter Anvin <hpa@zytor.com> 1.0.2-1
- Initial version.
