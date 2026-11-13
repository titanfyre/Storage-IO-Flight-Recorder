$ErrorActionPreference='Stop'
Add-Type -Path (Join-Path $PSScriptRoot 'lib/StorageIOFlightRecorderJsonSDKPowerShell.dll')
function Connect-StorageIOFlightRecorderSDK { <# .SYNOPSIS Opens an inert product SDK session. #> [CmdletBinding()]param() [TitanCSA.Products.StorageIOFlightRecorder.PowerShell.Client]::new() }
function Invoke-StorageIOFlightRecorderSDK { <# .SYNOPSIS Calls a documented product operation with an argument object. #> [CmdletBinding()]param([Parameter(Mandatory)]$Client,[Parameter(Mandatory)][string]$Operation,[hashtable]$Arguments=@{}) $Client.Call($Operation,($Arguments|ConvertTo-Json -Depth 100 -Compress)) | ConvertFrom-Json -Depth 100 }
function Disconnect-StorageIOFlightRecorderSDK { <# .SYNOPSIS Closes the SDK session and owned resources. #> [CmdletBinding()]param([Parameter(Mandatory)]$Client) $Client.Dispose() }
Export-ModuleMember -Function Connect-StorageIOFlightRecorderSDK,Invoke-StorageIOFlightRecorderSDK,Disconnect-StorageIOFlightRecorderSDK
