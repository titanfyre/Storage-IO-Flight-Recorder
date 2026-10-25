param([switch]$Asan,[string]$DriverPackageDirectory='')
$ErrorActionPreference='Stop'
$build=Join-Path $PSScriptRoot $(if($Asan){'obj\ASAN'}else{'obj\Build'})
$options=@('-S',$PSScriptRoot,'-B',$build,'-A','x64',('-DRECORDER_ASAN='+$(if($Asan){'ON'}else{'OFF'})))
if($Asan){
 $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
 $installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
 if(!$installation){throw 'Visual Studio C++ tools not found.'}
 $compiler=Get-ChildItem -LiteralPath (Join-Path $installation 'VC\Tools\MSVC') -Directory|Sort-Object {[version]$_.Name} -Descending|Select-Object -First 1
 $build=Join-Path $PSScriptRoot ('obj\Asan-'+$compiler.Name)
 $options[3]=$build
 $options+=@('-T',('version='+$compiler.Name))
}
if($DriverPackageDirectory){$options+=('-DDRIVER_PACKAGE_DIRECTORY='+[IO.Path]::GetFullPath($DriverPackageDirectory))}
& cmake.exe @options
if($LASTEXITCODE){throw 'CMake configuration failed.'}
$compile=@('--build',$build,'--config','Release','--parallel')
if($Asan){$compile+=@('--clean-first','--',('/p:VCToolsVersion='+$compiler.Name))}
& cmake.exe @compile
if($LASTEXITCODE){throw 'Native build failed.'}
if($Asan){
 $runtime=Join-Path $compiler.FullName 'bin\Hostx64\x64\clang_rt.asan_dynamic-x86_64.dll'
 if(!(Test-Path -LiteralPath $runtime)){throw 'The matching MSVC AddressSanitizer runtime was not found.'}
 Copy-Item -LiteralPath $runtime -Destination (Join-Path $build 'bin\Release') -Force
}
& ctest.exe --test-dir $build -C Release --output-on-failure
if($LASTEXITCODE){throw 'Offline tests failed.'}
