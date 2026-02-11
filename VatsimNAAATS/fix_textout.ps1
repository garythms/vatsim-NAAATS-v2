$files = Get-ChildItem -Path "D:\Development\vatsim-NAAATS-v2\VatsimNAAATS" -Recurse -Include *.cpp,*.h
foreach ($file in $files) {
    $content = Get-Content $file.FullName -Raw
    if ($content -match "->TextOutA") {
        $content = $content -replace "->TextOutA", "->TextOut"
        Set-Content -Path $file.FullName -Value $content -NoNewline
        Write-Host "Updated $($file.Name)"
    }
}