# Strace-Filebench  <br /> 
  使用環境:  <br />
  1. Ubuntu 14.04 x86  <br />
  2. Strace: [Trace Tool] strace-2.5.20  <br /> 
  3. Filebench: filebench  <br />
  <br />
  執行順序:  <br />
  1. 需要先執行filebench  <br />
  2. 再執行strace去取得workload的資料  <br />
  <br /> 
## Filebench  <br />
  <br />
## Strace  <br />
  1. 修改strace.c裡面.tr的路徑  <br />
     尋找strace.c裡的”.tr” <br />
  <br />
  2. 把路徑改成"指定路徑"  <br />
     由此,抓到的trace檔(.tr)會儲存在該路徑  <br />
  <br />
  3. 執行 $ ./strace -p “pid” -f <br />
  <br />
  -p pid  <br />
  跟踪指定的進程pid  <br />
  <br />
  -f 跟踪由fork呼叫所產生的子進程 <br />
  <br />
