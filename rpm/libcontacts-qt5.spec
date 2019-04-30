Name:       libcontacts-qt5
Summary:    Nemo contact cache library
Version:    0.2.3
Release:    1
Group:      System/Libraries
License:    BSD
URL:        https://github.com/nemomobile/libcontacts
Source0:    %{name}-%{version}.tar.bz2
Requires:   qtcontacts-sqlite-qt5
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Test)
BuildRequires:  pkgconfig(Qt5Contacts)
BuildRequires:  pkgconfig(Qt5Versit)
BuildRequires:  pkgconfig(mlite5)
BuildRequires:  pkgconfig(mlocale5)
BuildRequires:  pkgconfig(mce)
BuildRequires:  pkgconfig(qtcontacts-sqlite-qt5-extensions) >= 0.2.31
BuildRequires:  qt5-qttools
BuildRequires:  qt5-qttools-linguist

%description
%{summary}.

%package tests
Summary:    Nemo contact cache library tests
Group:      System/Libraries
Requires:   %{name} = %{version}-%{release}

%description tests
%{summary}.

%package devel
Summary:    Nemo contact cache library headers
Group:      System/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
%{summary}.


%package ts-devel
Summary: Translation source for libcontacts
Group: System/Applications

%description ts-devel
Translation source for libcontacts

%files ts-devel
%defattr(-,root,root,-)
%{_datadir}/translations/source/libcontacts.ts



%prep
%setup -q -n %{name}-%{version}

%build

%qmake5 VERSION=%{version}

make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%qmake5_install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_libdir}/libcontactcache-qt5.so*
%{_datadir}/translations/libcontacts_eng_en.qm

%files tests
%defattr(-,root,root,-)
/opt/tests/contactcache-qt5/*

%files devel
%defattr(-,root,root,-)
%{_includedir}/contactcache-qt5/*
%{_libdir}/pkgconfig/contactcache-qt5.pc
