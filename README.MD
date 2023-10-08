utility program for add per app sound device support to programs

windows only (only tested on win10)

usage: arrexe.exe <ProgramToExecute> <ProgramArgs>

uses microsoft/Detours to intercept CoCreateInstance calls creating an IMMDeviceEnumerator instance
to change GetDefaultAudioEndpoint output by querying the registry for DefaultEndpoint entries.

only tested with an old unity version, but probably works for any app
using IMMDeviceEnumerator to query the DefaultAudioEndpoint
