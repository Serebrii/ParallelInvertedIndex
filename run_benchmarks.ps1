[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

$exePath = ".\x64\Release\IndexBuilder.exe"
$inputDir = "D:\Projects\University\4 course 1 sem\Parallel computing methods\Lab1\TestData"
$outputDir = "D:\Projects\University\4 course 1 sem\Parallel computing methods\Lab1\Outputs"
$csvFile = "results.csv"

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

"Version,Threads,K,RunTime_ms,Speedup,Efficiency,KarpFlatt" | Out-File -FilePath $csvFile -Encoding utf8

function Extract-MedianTime($outputLines) {
    foreach ($line in $outputLines) {
        if ($line -match ":\s*([\d\.]+)\s*ms") {
            return [double]$Matches[1]
        }
    }
    return 0.0
}

Write-Host "=== Running V0 ===" -ForegroundColor Green
$v0Out = & $exePath --version V0 --input $inputDir --output "$outputDir\v0.txt" --runs 5
$v0Time = Extract-MedianTime $v0Out
Write-Host "V0 Time: $v0Time ms" -ForegroundColor Gray
"V0,1,0,$v0Time,1.00,1.00,0.00" | Out-File -FilePath $csvFile -Append -Encoding utf8

$threadsList = @(1, 2, 4, 6, 8, 12)
$versions = @("V2", "V4_seq", "V4_tree")

foreach ($v in $versions) {
    foreach ($p in $threadsList) {
        Write-Host "=== Running ${v} (P=${p}) ===" -ForegroundColor Cyan
        $out = & $exePath --version $v --input $inputDir --output "$outputDir\${v}_P${p}.txt" --threads $p --runs 5
        $time = Extract-MedianTime $out
        
        $speedup = 0.0
        $efficiency = 0.0
        $karpFlatt = 0.0

        if ($time -gt 0) {
            $speedup = $v0Time / $time
            $efficiency = $speedup / $p
            if ($p -gt 1) {
                $karpFlatt = ((1.0 / $speedup) - (1.0 / $p)) / (1.0 - (1.0 / $p))
            }
        }

        Write-Host "${v} (P=${p}): $time ms | Speedup: $speedup" -ForegroundColor Gray
        "${v},${p},0,${time},${speedup},${efficiency},${karpFlatt}" | Out-File -FilePath $csvFile -Append -Encoding utf8
    }
}

$kList = @(1, 4, 16, 64, 256, 1024, 4096)
foreach ($k in $kList) {
    Write-Host "=== Running V3 (P=4, K=${k}) ===" -ForegroundColor Yellow
    $out = & $exePath --version V3 --input $inputDir --output "$outputDir\V3_K${k}.txt" --threads 4 -K $k --runs 5
    $time = Extract-MedianTime $out
    
    $speedup = 0.0
    $efficiency = 0.0
    $karpFlatt = 0.0

    if ($time -gt 0) {
        $speedup = $v0Time / $time
        $efficiency = $speedup / 4
        $karpFlatt = ((1.0 / $speedup) - (1.0 / 4)) / (1.0 - (1.0 / 4))
    }

    Write-Host "V3 (K=${k}): $time ms | Speedup: $speedup" -ForegroundColor Gray
    "V3,4,${k},${time},${speedup},${efficiency},${karpFlatt}" | Out-File -FilePath $csvFile -Append -Encoding utf8
}

Write-Host "`n[DONE] Benchmark complete. Results saved to $csvFile" -ForegroundColor Green