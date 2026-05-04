# =========================
# IM Benchmark Windows Setup
# Run as Administrator
# =========================

Write-Host "Setting dynamic port range..." -ForegroundColor Cyan
netsh int ipv4 set dynamicport tcp start=10000 num=55000

Write-Host "Creating firewall rule for port 8888..." -ForegroundColor Cyan
$ruleName = "IM Bench 8888"
if (-not (Get-NetFirewallRule -DisplayName $ruleName -ErrorAction SilentlyContinue)) {
    New-NetFirewallRule `
        -DisplayName $ruleName `
        -Direction Inbound `
        -LocalPort 8888 `
        -Protocol TCP `
        -Action Allow
}

Write-Host "Done." -ForegroundColor Green