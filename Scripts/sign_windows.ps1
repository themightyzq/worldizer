# TEMPLATE - Authenticode-sign the Worldizer VST3 for Windows distribution.
# Not yet wired into CI. Requires a code-signing certificate (OV or EV) either in
# the certificate store or as a PFX file, and signtool.exe from the Windows SDK.
#
# Usage (PFX file):
#   .\Scripts\sign_windows.ps1 -PfxPath cert.pfx -PfxPassword (Read-Host -AsSecureString)
# Usage (store, by subject name):
#   .\Scripts\sign_windows.ps1 -SubjectName "ZQSFX"

param(
    [string]$Vst3Path   = "build\Worldizer_artefacts\Release\VST3\Worldizer.vst3\Contents\x86_64-win\Worldizer.vst3",
    [string]$PfxPath    = "",
    [SecureString]$PfxPassword,
    [string]$SubjectName = "",
    [string]$TimestampUrl = "http://timestamp.digicert.com"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $Vst3Path)) {
    throw "VST3 binary not found: $Vst3Path (build Release first)"
}

$args = @("sign", "/fd", "SHA256", "/tr", $TimestampUrl, "/td", "SHA256")

if ($PfxPath) {
    $plain = [Runtime.InteropServices.Marshal]::PtrToStringAuto(
        [Runtime.InteropServices.Marshal]::SecureStringToBSTR($PfxPassword))
    $args += @("/f", $PfxPath, "/p", $plain)
} elseif ($SubjectName) {
    $args += @("/n", $SubjectName)
} else {
    throw "Provide -PfxPath or -SubjectName"
}

$args += $Vst3Path

Write-Host "Signing $Vst3Path ..."
& signtool.exe @args

Write-Host "Verifying ..."
& signtool.exe verify /pa /v $Vst3Path

Write-Host "Done."
