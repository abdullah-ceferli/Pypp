[Setup]
AppName=My Great App
AppVersion=1.0
DefaultDirName={autopf}\MyGreatApp
DefaultGroupName=My Great App
; This creates the "Install" button flow
OutputDir=userdocs:\Output

[Files]
; Source is the file on your PC, DestDir is where it goes on the user's PC
Source: "C:\YourFolder\YourProgram.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\My Great App"; Filename: "{app}\YourProgram.exe"

[Code]
// If you want custom logic for the buttons, you can script it here, 
// but Inno Setup provides the "Change Directory" and "Install" buttons by default.