[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Launcher,
    [Parameter(Mandatory = $true)][string]$Runtime
)

$ErrorActionPreference = 'Stop'
if ((Get-AuthenticodeSignature -LiteralPath $Launcher).Status -ne 'NotSigned') {
    throw 'Prepare executable metadata before signing. The launcher was not modified.'
}

if (-not ('PharosRelease.VersionResource' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;
namespace PharosRelease {
    public static class VersionResource {
        delegate bool LanguageCallback(IntPtr module, IntPtr type, IntPtr name, ushort language, IntPtr data);
        [DllImport("kernel32", CharSet=CharSet.Unicode, SetLastError=true)]
        static extern IntPtr LoadLibraryEx(string file, IntPtr reserved, uint flags);
        [DllImport("kernel32", SetLastError=true)] static extern bool FreeLibrary(IntPtr module);
        [DllImport("kernel32", CharSet=CharSet.Unicode, SetLastError=true)]
        static extern IntPtr FindResource(IntPtr module, IntPtr name, IntPtr type);
        [DllImport("kernel32", SetLastError=true)] static extern uint SizeofResource(IntPtr module, IntPtr resource);
        [DllImport("kernel32", SetLastError=true)] static extern IntPtr LoadResource(IntPtr module, IntPtr resource);
        [DllImport("kernel32")] static extern IntPtr LockResource(IntPtr resource);
        [DllImport("kernel32", CharSet=CharSet.Unicode, SetLastError=true)]
        static extern bool EnumResourceLanguages(IntPtr module, IntPtr type, IntPtr name, LanguageCallback callback, IntPtr data);
        [DllImport("kernel32", CharSet=CharSet.Unicode, SetLastError=true)]
        static extern IntPtr BeginUpdateResource(string file, bool deleteExisting);
        [DllImport("kernel32", CharSet=CharSet.Unicode, SetLastError=true)]
        static extern bool UpdateResource(IntPtr update, IntPtr type, IntPtr name, ushort language, byte[] data, uint size);
        [DllImport("kernel32", CharSet=CharSet.Unicode, SetLastError=true)]
        static extern bool EndUpdateResource(IntPtr update, bool discard);
        static readonly IntPtr Type = new IntPtr(16), Name = new IntPtr(1);
        static IntPtr Open(string file) {
            IntPtr result = LoadLibraryEx(file, IntPtr.Zero, 2);
            if (result == IntPtr.Zero) throw new Win32Exception();
            return result;
        }
        public static void Copy(string source, string destination) {
            byte[] bytes;
            IntPtr module = Open(source);
            try {
                IntPtr resource = FindResource(module, Name, Type);
                if (resource == IntPtr.Zero) throw new Win32Exception();
                uint length = SizeofResource(module, resource);
                IntPtr address = LockResource(LoadResource(module, resource));
                if (address == IntPtr.Zero || length == 0) throw new Win32Exception();
                bytes = new byte[length];
                Marshal.Copy(address, bytes, 0, checked((int)length));
            } finally { FreeLibrary(module); }
            var languages = new List<ushort>();
            module = Open(destination);
            try {
                EnumResourceLanguages(module, Type, Name, (m,t,n,l,d) => { languages.Add(l); return true; }, IntPtr.Zero);
            } finally { FreeLibrary(module); }
            if (languages.Count == 0) languages.Add(1033);
            IntPtr update = BeginUpdateResource(destination, false);
            if (update == IntPtr.Zero) throw new Win32Exception();
            bool commit = false;
            try {
                foreach (ushort language in languages)
                    if (!UpdateResource(update, Type, Name, language, bytes, (uint)bytes.Length)) throw new Win32Exception();
                commit = true;
            } finally {
                if (!EndUpdateResource(update, !commit)) throw new Win32Exception();
            }
        }
    }
}
'@
}
[PharosRelease.VersionResource]::Copy(
    [IO.Path]::GetFullPath($Runtime), [IO.Path]::GetFullPath($Launcher))
