Rev 1   09/24/2010 JC  Change to "bin" directory. Use double quotes in case filename contains spaces.

Options -> Environment Options
    Execute Keys
       [Add]
       Enter extensions seperated with commas (no dot): dp

[F9]   Execute: "C:\Program Files\TEV\AXI\bin\ddc32.exe"
       Start in:
       Parameters: %n -o "%p\%F.do" -L "%p\%F_errors.txt"
       Window: Normal
       Hint:  ddcd32
       Save:  Current file before execution

       Capture console output


[F10]  Execute: "C:\Program Files\TEV\AXI\bin\ddrc32.exe"
       Start in:
       Parameters: "%p%F.do" -o "%p%F_r.dp"
       Window: Normal
       Hint:  DD48 Re-Compiler of .do
       Save:  Nothing

       Capture console output


[F11]  Execute: "C:\Program Files\ConTEXT\ConTEXT.exe"
       Start in:
       Parameters:  "%p%F_r.dp"
       Window: Normal
       Hint:  Open the recompiled file
       Save:  Nothing

       Capture console output

[F12]  Execute: "C:\Program Files\TEV\AXI\Doc\TEV DD Pattern Language.doc"
       Start in:
       Parameters:  
       Window: Normal
       Hint:  DD48 Pattern Language Manual
       Save:  Nothing

       don't Capture console output





       Enter extensions seperated with commas (no dot): mvp

[F9]   Execute: "C:\Program Files\TEV\AXI\bin\MVPEditor.exe"
       Start in:
       Parameters: %n
       Window: Normal
       Hint:  MVP Editor
       Save:  Current file before execution

       Capture console output


[F12]  Execute: "C:\Program Files\TEV\AXI\Doc\AXI MVP API Spec.doc"
       Start in:
       Parameters:  
       Window: Normal
       Hint:  MVP API Manual
       Save:  Nothing

       don't Capture console output



FEATURES:
unlimited open files
unlimited editing file size, 4kB line length
powerful syntax highlighting for:
C#
C/C++
Delphi/Pascal
Java
Java Script
Visual Basic
Perl/CGI
HTML
CSS
SQL
FoxPro
80x86 assembler
Python
PHP
Tcl/Tk
XML
Fortran
Foxpro
InnoSetup scripts
powerful custom defined syntax highlighter
multilanguage support
English
German
French
Croatian
Chinese
Czech
Danish
Dutch
Estonian
Esperanto
Spanish
Galego
Italian
Hungarian
Portuguese (Brazil)
Russian
Slovakian
Polish
Lithuanian
Latvian
Slovenian
Turkish
project workspaces support
unicode UTF8 support
code templates
customizable help files for each file type
file explorer with favorites list
file compare
export to HTML/RTF
conversion DOS->UNIX->Macintosh file formats
editing position remembering across files
macro recorder
commenting/uncommenting code
text sort
normal and columnar text selection
bookmarks
search and replace with regular expressions
search and replace text in all open files
incremental search with text emphasizing
C/Java-style block auto indent/outdent
customizable color printing with print preview
exporting configuration stored in registry
customizable syntax highlighter colors, cursors, margin, gutter, line spacing...
user definable execution keys, depending on file type 
capturing console applications standard output
compiler output parser for positioning on error line
powerful command line handler 