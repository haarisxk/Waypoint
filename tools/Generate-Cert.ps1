$ErrorActionPreference = "Stop"

$RootSubject = "CN=Waypoint Root Certificate Authority, O=Made by Haaris Khan"
$PublisherSubject = "CN=Waypoint Certificate, O=Made by Haaris Khan"
$CertPath = "$PSScriptRoot\..\cert\Waypoint.cer"

Write-Host "Generating Waypoint Root Certificate Authority (20-year lifespan)..." -ForegroundColor Cyan
$RootCert = New-SelfSignedCertificate -Type Custom `
    -KeySpec Signature `
    -Subject $RootSubject `
    -KeyExportPolicy Exportable `
    -HashAlgorithm sha256 `
    -KeyLength 2048 `
    -CertStoreLocation "Cert:\CurrentUser\My" `
    -KeyUsageProperty Sign `
    -KeyUsage CertSign, CRLSign `
    -NotAfter (Get-Date).AddYears(20) `
    -TextExtension @("2.5.29.19={text}CA=true&pathlength=1")

Write-Host "Generating Waypoint Code Signing Certificate (10-year lifespan)..." -ForegroundColor Cyan
$CodeSigningCert = New-SelfSignedCertificate -Type Custom `
    -KeySpec Signature `
    -Subject $PublisherSubject `
    -KeyExportPolicy Exportable `
    -HashAlgorithm sha256 `
    -KeyLength 2048 `
    -CertStoreLocation "Cert:\CurrentUser\My" `
    -Signer $RootCert `
    -NotAfter (Get-Date).AddYears(10) `
    -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3")

Write-Host "Exporting Root CA to $CertPath..." -ForegroundColor Cyan
Export-Certificate -Cert $RootCert -FilePath $CertPath -Force | Out-Null

Write-Host "`nSuccessfully created PKI Infrastructure for Waypoint!" -ForegroundColor Green
Write-Host "Root CA Thumbprint:      $($RootCert.Thumbprint)"
Write-Host "Code Signing Thumbprint: $($CodeSigningCert.Thumbprint)"
Write-Host "Saved public Root CA to: $CertPath"
