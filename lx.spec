Name:           lx
Version:        20030328
Release:        5.1%{?dist}
Summary:        Converts PBM data to Lexmark 1000 printer language

Group:          System Environment/Libraries
License:        GPL+
URL:            http://moinejf.free.fr/
Source0:        http://moinejf.free.fr/%{name}.c
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

# Original driver this one was based upon
Conflicts:      lm1100

%description
This is a filter to convert PBM data such as produced by ghostscript to
the printer language of Lexmark 1000 printers.  It is meant to be used
by the PostScript Description files of the drivers from the foomatic package.

%prep
%setup -T -c
#%{__cp} %{_sourcedir}/lx.c %{_builddir}/%{name}-%{version}/
%{__cp} %{SOURCE0} %{_builddir}/%{name}-%{version}/

%build
%{__cc} %{optflags} -o lm1100 lx.c

%install
rm -rf $RPM_BUILD_ROOT
%{__mkdir} -p $RPM_BUILD_ROOT/%{_bindir}
%{__install} lm1100 $RPM_BUILD_ROOT/%{_bindir}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{_bindir}/lm1100

%changelog
* Mon Nov 30 2009 Dennis Gregorovic <dgregor@redhat.com> - 20030328-5.1
- Rebuilt for RHEL 6

* Sat Jul 25 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 20030328-5
- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild

* Wed Feb 25 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 20030328-4
- Rebuilt for https://fedoraproject.org/wiki/Fedora_11_Mass_Rebuild

* Tue Feb 19 2008 Fedora Release Engineering <rel-eng@fedoraproject.org> - 20030328-3
- Autorebuild for GCC 4.3

* Fri Aug 3 2007 Lubomir Kundrak <lkundrak@redhat.com> 20030328-2
- Modify the License tag in accordance with the new guidelines

* Thu Jun 7 2007 Lubomir Kundrak <lkundrak@redhat.com> 20030328-1
- Initial package
