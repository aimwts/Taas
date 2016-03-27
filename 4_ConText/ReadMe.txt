

ConTEXT
~~~~~~~


  Introduction
  ~~~~~~~~~~~~

    ConTEXT is a small, fast and powerful text editor for software developers.
    After years and years gumbling with all kind of Windows text editors, We
    haven't found any of them to complete satisfy our needs, so we wrote my own.

    This editor is freeware, absolutely free for use. If you are so fascinated
    and want to pay for it, I encourage you to send any amount of anything to
    the address listed at the bottom of this file. Anyway, I'd like to hear any
    of your comments and suggestions, to discuss it and implement it in future
    versions.

    Check ConTEXT pages at http://www.contexteditor.org for ConTEXT updates.


  Features
  ~~~~~~~~

    * unlimited open files
    * unlimited editing file size, 4kB line length
    * powerful syntax highlighting for:
        - C/C++
        - Delphi/Pascal
        - Java
        - Java Script
        - Visual Basic
        - Perl/CGI
        - HTML
        - SQL
        - 80x86 assembler
        - Python
        - PHP
        - TCL/Tk
        - User customizable syntax highlighters
    * multilanguage support
    * project workspaces support
    * unicode UTF8 support
    * code templates
    * customizable help files for each file type
    * file explorer with favorites list
    * export to HTML/RTF
    * conversion DOS->UNIX->Macintosh file formats
    * editing position remembering accross files
    * macro recorder
    * commenting/uncommenting code
    * text sort
    * normal and columnar text selection
    * bookmarks
    * find and replace text in all open files
    * C/Java-style block auto indent/outdent
    * customizable color printing with print preview
    * exporting configuration stored in registry
    * customizable syntax highlighting colors, cursor shapes, right margin,
       gutter, line spacing...
    * user defineable execution keys, depending on file type
    * capturing console applications standard output
    * powerful command line handler
    * install and uninstall
    * it's FREE!



  Planned features for v1.0
  ~~~~~~~~~~~~~~~~~~~~~~~~~

    * hex editor
    * plug-in architecture for external tools
    * enhancing macro recorder features and macro script language
    * code browser for C/C++, Delphi and Visual Basic projects
    * hard tabs support
    * paragraphs and real word wrapping
    * more powerful custom highlighter definition language
    * file compare
    * regular expressions in find/replace dialogs
    * other misc. tools



  What's new in this version?
  ~~~~~~~~~~~~~~~~~~~~~~~~~~

    Read History.txt.



  Original NOTEPAD.EXE replacement
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    Since many applications has hardcoded calling Windows Notepad.exe, the only
    way to start ConTEXT instead original Windows Notepad is to cheat those
    applications by replacing original Windows Notepad with other fake-Notepad
    which will call ConTEXT everytime Notepad is started. To achieve this,
    follow next steps:

      1. Back-up your original notepad.exe found in C:\Windows\ (Windows 95/98)
         or in C:\WinNT\System32 (Windows NT) by renaming it to e.g.
         Notepad.bak
      2. Copy Notepad.exe found in directory where ConTEXT is installed
         (e.g. C:\Program Files\ConTEXT\) to C:\Windows or C:\WinNT\System32\

    For Windows 2000 users, following procedure is required:

      1. Rename C:\WinNT\System32\DLLCache\Notepad.exe to Notepad.bak. If you
         don't see C:\WinNT\System32\DLLCache folder in explorer, start MS-DOS
         prompt and type:

         cd c:\WinNT\system32\dllcache\
         ren notepad.exe notepad.bak

      2. Replace C:\WinNT\Notepad.exe and C:\WinNT\System32\Notepad.exe with
         Notepad.exe found in directory where ConTEXT is installed.
      3. Windows will tell you that a system file has been replaced and ask you
         for the CD (since it can't refresh it automatically from dllcache).
         Just cancel the dialog and then press YES to let it know you want to
         keep the new file.



  Custom highlighters
  ~~~~~~~~~~~~~~~~~~~

    To define custom syntax highlighter, copy Highlighters/x86 Assembler.chl
    to new file and edit it. File is well commented and there should be no
    problems understanding it.

    Check ConTEXT support pages for additional highlighters.

    Note: Install highlighters you need. Lots of custom highlighters can
          slightly increase ConTEXT loading time.



  Code templates
  ~~~~~~~~~~~~~~

    Code template is a set of shortcuts with associated code. It is used for
    most frequently used code structures. Templates are stored in directory
    C:\Program Files\ConTEXT\Template. Pipe char (|) defines where cursor
    will be positioned after inserting code from template. See ObjectPascal
    template for example.

    If you have your own templates, send it to me to include it in next
    ConTEXT distributions.



  Multilanguage support
  ~~~~~~~~~~~~~~~~~~~~~

    All language files are stored in language\ subdirectory in direcotry where
    ConTEXT is installed. If translation to your language doesn't exists and
    you want to contribute translation, please be free to translate
    language\Custom.lng file and e-mail it to info@contexteditor.org and it will
    be included in next ConTEXT distribution. Also, I'd be glad if you send me
    any corrections to existing translations.



  Upgrading ConTEXT
  ~~~~~~~~~~~~~~~~~

    To get newest ConTEXT version, language updates or custom highlighters,
    please check:

    http://www.contexteditor.org

    There is no need to uninstall old ConTEXT version prior to upgrading to
    newer version. When ConTEXT is uninstalled, all environment settings will
    remain in registry and will be used in newer version.



  Standard disclaimer
  ~~~~~~~~~~~~~~~~~~~

    This program is distributed as freeware. This software is provided "as is",
    without any guarantee made as to its suitability or fitness for any
    particular use. It may contain bugs, so use of this tool is at your own
    risk. Author takes no responsibility for any damage that may unintentionally
    be caused through its use. You may not distribute ConTEXT in any form
    without express written permission of author.



  Author
  ~~~~~~

    Postal Address:   ConTEXT Project Ltd
                      The Meridian,
                      4 Copthall House,
                      Station Square,
                      Coventry,
                      West Midlands,
                      CV1 2FL
                      UK

    Email:            info@contexteditor.org
    Web:              http://www.contexteditor.org