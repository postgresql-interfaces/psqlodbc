---
title: psqlODBC
layout: hextra-home
cascade:
  - target:
      path: /changelog
    title: Changelog
    weight: 4
    type: docs
---

{{< hextra/hero-headline >}}
  psqlODBC
{{< /hextra/hero-headline >}}

{{< hextra/hero-subtitle >}}
  The official PostgreSQL ODBC driver
{{< /hextra/hero-subtitle >}}

<div class="hx:mt-6 hx:mb-6">
{{< hextra/hero-button text="Documentation" link="docs" >}}
{{< hextra/hero-button text="Download" link="https://www.postgresql.org/ftp/odbc/releases/" style="background: none; color: inherit; border: 1px solid;" >}}
</div>

<div class="content">

**psqlODBC** is the official PostgreSQL ODBC Driver. It is released under the [Library General Public Licence](https://opensource.org/license/lgpl-license-html/), or LGPL.

Downloads are available in source and binary formats at the [PostgreSQL downloads site](https://www.postgresql.org/ftp/odbc/releases/)

------------------------------------------------------------------------

## psqlODBC Documentation

The following documents contain various bits of useful information. Please send any additional documentation, or report errors/omissions to <pgsql-odbc@postgresql.org>

- [Notes on the various configuration settings.](/docs/configuration/)
- [Compiling psqlODBC on Win32 systems.](/docs/building-windows/)
- [Compiling psqlODBC on Unix systems.](/docs/building-unix/)
- [Frequently Asked Questions (FAQ).](/faq/)
- [Release notes.](/changelog/)

------------------------------------------------------------------------

## psqlODBC HOWTOs

The following HOWTOs have been contributed by various people. If you wish to add to the collection, please send your contribution to <pgsql-odbc@postgresql.org>

- [Example based Mini-Howto on Accessing PostgreSQL from C#](/howto/csharp/)
- [Example based Mini-Howto on Accessing PostgreSQL from Ch](/howto/ch/)
- [Using large objects in Microsoft Access (notes from the original psqlODBC docs)](/howto/access-large-objects/)
- [Example based Mini-Howto on using Microsoft Access VBA with PostgreSQL](/howto/access-vba/)
- [Example based Mini-Howto on Accessing PostgreSQL from Visual Basic](/howto/visual-basic/)
- [Example based Mini-Howto on Pgsql Large Objects Interface and Visual Basic](/howto/visual-basic-large-objects/)
- [PostgreSQL and Business Objects](/howto/business-objects/)

------------------------------------------------------------------------

## psqlODBC Development

Previously psqlODBC was developed and supported through the [pgsql-odbc@postgresql.org](http://www.postgresql.org/list/pgsql-odbc/) mailing list.

However we have moved the project to [GitHub psqlODBC](https://github.com/postgresql-interfaces/psqlodbc) please file issues [here](https://github.com/postgresql-interfaces/psqlodbc/issues)

We will continue to monitor the pgsql-odbc mailing list however it's preferable to use GitHub.

You can browse the source code at the psqlODBC git repository at [postgresql-interfaces/psqlodbc](https://github.com/postgresql-interfaces/psqlodbc). The source for this website is in the [website](https://github.com/postgresql-interfaces/psqlodbc/tree/main/website) directory of that repository.

------------------------------------------------------------------------

## psqlODBC History

The source code for this driver was originally adopted from version 0.20 of PostODBC. The authors at that time were Christian Czezatke and Dan McGuirk. Later it was maintained by Julie Ann Case.

As part of a commercial research project, the developers at Insight Distributions System overhauled the driver. Their goal was to make the driver commercially viable. In keeping with the spirit with which the original source was acquired, Insight published their work at their website. Some time later and after some discussion with members of the PostgreSQL organization, psqlODBC was adapted as the part of the PostgreSQL source distribution. The driver continued to be maintained by Byron Nikolaidis, the developer at Insight who overhauled the driver for some time.

The driver is currently maintained by a number of contributors to the PostgreSQL project.

------------------------------------------------------------------------

## Acknowledgements

### OpenSSL

This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit (<http://www.openssl.org/>).

### MIT Kerberos

This product includes Kerberos software developed by the Massachusetts Institute of Technology (<http://web.mit.edu/kerberos/>):

Copyright 1992-2004 by the Massachusetts Institute of Technology. All rights reserved.

THIS SOFTWARE IS PROVIDED "AS IS", AND M.I.T. MAKES NO REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED. By way of example, but not limitation, M.I.T. MAKES NO REPRESENTATIONS OR WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR THAT THE USE OF THE LICENSED SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.

</div>
