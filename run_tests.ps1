param(
    [string]$AlgorithmName = "solution"
)

$ErrorActionPreference = "Continue"
$root = $PSScriptRoot
if (-not $root) { $root = (Get-Location).Path }

$cppFile    = Join-Path $root "$AlgorithmName.cpp"
$exeFile    = Join-Path $root "$AlgorithmName.exe"
$checkerExe = Join-Path $root "checker.exe"
$inputDir   = Join-Path $root "input"
$outputDir  = Join-Path $root "output"
$expectedDir = Join-Path $root "input\expected"
$resultsFile = Join-Path $root "${AlgorithmName}_results.txt"

# ── Prevajanje ────────────────────────────────────────────────────────────────
Write-Host "Prevajanje $AlgorithmName.cpp ..." -ForegroundColor Cyan
$compileOut = & g++ -O2 -std=c++17 -o $exeFile $cppFile 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "NAPAKA pri prevajanju:" -ForegroundColor Red
    $compileOut | ForEach-Object { Write-Host "  $_" }
    exit 1
}
Write-Host "Prevajanje uspesno." -ForegroundColor Green

if (-not (Test-Path $checkerExe)) {
    Write-Host "Prevajanje checker.cpp ..." -ForegroundColor Cyan
    & g++ -O2 -std=c++17 -o $checkerExe (Join-Path $root "checker.cpp") 2>&1 | Out-Null
}

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

# ── Glava rezultatov ─────────────────────────────────────────────────────────
$header = @"
Algoritem : $AlgorithmName
Datum     : $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')

$(("{0,-38} {1,9}  {2,6}  {3,-7}  {4}" -f "Test", "Cas(ms)", "Parov", "Veljav.", "Pricakovano"))
$("-" * 80)
"@
$header | Out-File $resultsFile -Encoding utf8

$totalTests  = 0
$passedCount = 0
$totalMs     = 0

# ── Zanka po vhodnih datotekah ────────────────────────────────────────────────
Get-ChildItem "$inputDir\*.txt" | Sort-Object Name | ForEach-Object {
    $base   = $_.BaseName
    $inFile = $_.FullName
    $outFile = Join-Path $outputDir "${base}-out.txt"

    # Pozeni algoritem in izmeri cas (timeout 10000ms)
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $proc = Start-Process -FilePath $exeFile -ArgumentList "`"$inFile`"", "`"$outFile`"" -PassThru -NoNewWindow
    $finished = $proc.WaitForExit(10000)
    $sw.Stop()
    $ms = $sw.ElapsedMilliseconds
    $timedOut = $false
    if (-not $finished) {
        $proc.Kill()
        $timedOut = $true
    }
    $totalMs += $ms

    # Stevilo najdenih parov
    $pairs = "?"
    if (Test-Path $outFile) {
        $firstLine = Get-Content $outFile -TotalCount 1 -ErrorAction SilentlyContinue
        if ($firstLine -match '^\d+$') { $pairs = $firstLine.Trim() }
    }

    # Preverimo veljavnost z checkerjem
    $valid = "N/A"
    if ((Test-Path $outFile) -and (Test-Path $checkerExe)) {
        $chk = & $checkerExe $inFile $outFile 2>&1 | Out-String
        if ($chk -match "veljavna") { $valid = "OK" } else { $valid = "FAIL" }
    }

    # Poiscemo pricakovano datoteko (oba mozna formata imen)
    $expFile = $null
    $candidates = @(
        (Join-Path $expectedDir "${base}-expected.txt"),
        (Join-Path $expectedDir "${base}_solution.txt"),
        (Join-Path $expectedDir "${base}-out.txt")
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { $expFile = $c; break }
    }

    $vsExp = "ni pricakovanega"
    if ($expFile) {
        $expCount = (Get-Content $expFile -TotalCount 1 -ErrorAction SilentlyContinue).Trim()
        $totalTests++
        if ($pairs -eq $expCount) {
            $vsExp = "UJEMA ($expCount)"
            $passedCount++
        } else {
            $vsExp = "RAZLIKA  exp=$expCount  dobljeno=$pairs"
        }
    }

    if ($timedOut) { $valid = "TIMEOUT"; $pairs = "?" }

    $line = "{0,-38} {1,9}  {2,6}  {3,-7}  {4}" -f $base, $ms, $pairs, $valid, $vsExp
    $line | Add-Content $resultsFile -Encoding utf8

    # Barvni izpis v konzolo
    $color = if ($valid -eq "TIMEOUT") { "Magenta" }
             elseif ($valid -eq "FAIL") { "Red" }
             elseif ($vsExp -like "RAZLIKA*") { "Yellow" }
             else { "White" }
    Write-Host $line -ForegroundColor $color
}

# ── Povzetek ──────────────────────────────────────────────────────────────────
$summary = @"

$("-" * 80)
Skupni cas  : ${totalMs} ms
Testov z ref: $passedCount / $totalTests ujemajocih
"@
$summary | Add-Content $resultsFile -Encoding utf8
Write-Host $summary -ForegroundColor Cyan
Write-Host "Rezultati shranjeni v: $resultsFile" -ForegroundColor Green
