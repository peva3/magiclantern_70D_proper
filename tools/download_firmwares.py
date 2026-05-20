#!/usr/bin/env python3
"""Download all Canon firmware files from eoscard.pel.hu"""
import re
import os
import time
import urllib.request
import urllib.error
import sys

BASE_URL = "https://pel.hu/down/"
FIRMWARE_DIR = "/app/70d/firmware"

# Firmware links extracted from eoscard.pel.hu HTML
# Structure: (folder_name, [(version_label, url), ...])
CAMERAS = [
    ("EOS_1D_X_MkIII", [
        ("1.1.0", "https://pel.hu/down/eos1dx3-v110-win.zip"),
        ("1.2.0", "https://pel.hu/down/eos1dx3-v120-win.zip"),
        ("1.2.1", "https://pel.hu/down/eos1dx3-v121-win.zip"),
        ("1.3.0", "https://pel.hu/down/eos1dx3-v130-win.zip"),
        ("1.4.0", "https://pel.hu/down/eos1dx3-v140-win.zip"),
        ("1.5.0", "https://pel.hu/down/eos1dx3-v150-win.zip"),
        ("1.6.0", "https://pel.hu/down/eos1dx3-v160-win.zip"),
        ("1.6.1", "https://pel.hu/down/eos1dx3-v161-win.zip"),
        ("1.6.2", "https://pel.hu/down/eos1dx3-v162-win.zip"),
        ("1.7.1", "https://pel.hu/down/eos1dx3-v171-win.zip"),
        ("1.8.0", "https://pel.hu/down/eos1dx3-v180-win.zip"),
    ]),
    ("EOS_1D_X_MkII", [
        ("1.0.2", "https://pel.hu/down/eos1dx2-v102-win.zip"),
        ("1.1.2", "https://pel.hu/down/eos1dx2-v112-win.zip"),
        ("1.1.3", "https://pel.hu/down/eos1dx2-v113-win.zip"),
        ("1.1.4", "https://pel.hu/down/eos1dx2-v114-win.zip"),
        ("1.1.6", "https://pel.hu/down/eos1dx2-v116-win.zip"),
        ("1.1.7", "https://pel.hu/down/eos1dx2-v117-win.zip"),
        ("1.1.8", "https://pel.hu/down/eos1dx2-v118-win.zip"),
    ]),
    ("EOS_1D_X", [
        ("1.0.6", "https://pel.hu/down/eos1dx-v106-win.zip"),
        ("1.1.1", "https://pel.hu/down/eos1dx-v111-win.zip"),
        ("1.2.1", "https://pel.hu/down/eos1dx-v121-win.zip"),
        ("1.2.4", "https://pel.hu/down/eos1dx-v124-win.zip"),
        ("2.0.3", "https://pel.hu/down/eos1dx-v203-win.zip"),
        ("2.0.7", "https://pel.hu/down/eos1dx-v207-win.zip"),
        ("2.0.8", "https://pel.hu/down/eos1dx-v208-win.zip"),
        ("2.0.9", "https://pel.hu/down/eos1dx-v209-win.zip"),
        ("2.1.0", "https://pel.hu/down/eos1dx-v210-win.zip"),
        ("2.1.1", "https://pel.hu/down/eos1dx-v211-win.zip"),
    ]),
    ("EOS_1Ds_MkIII", [
        ("1.2.0", "https://pel.hu/down/eos1dsmk3120.exe"),
        ("1.2.2", "https://pel.hu/down/eos1ds3-v122-win.zip"),
        ("1.2.3", "https://pel.hu/down/eos1ds3-v123-win.zip"),
    ]),
    ("EOS_1D_MkIV", [
        ("1.0.6", "https://pel.hu/down/eos1dmk4106.exe"),
        ("1.0.8", "https://pel.hu/down/eos1dmk4108.exe"),
        ("1.1.0", "https://pel.hu/down/eos1d4-v110-win.exe"),
        ("1.1.1", "https://pel.hu/down/eos1d4-v111-win.zip"),
        ("1.1.3", "https://pel.hu/down/eos1d4-v113-win.zip"),
        ("1.1.4", "https://pel.hu/down/eos1d4-v114-win.zip"),
        ("1.1.5", "https://pel.hu/down/eos1d4-v115-win.zip"),
    ]),
    ("EOS_1D_MkIII", [
        ("1.1.3", "https://pel.hu/down/eos1dmk3113.exe"),
        ("1.3.0", "https://pel.hu/down/eos1dmk3130.exe"),
        ("1.3.1", "https://pel.hu/down/eos1d3-v131-win.zip"),
        ("1.3.2", "https://pel.hu/down/eos1d3-v132-win.zip"),
    ]),
    ("EOS_5DS", [
        ("1.1.0", "https://pel.hu/down/eos5ds-v110-win.zip"),
        ("1.1.1", "https://pel.hu/down/eos5ds-v111-win.zip"),
        ("1.1.2", "https://pel.hu/down/eos5ds-v112-win.zip"),
        ("1.1.3", "https://pel.hu/down/eos5ds-v113-win.zip"),
    ]),
    ("EOS_5DSR", [
        ("1.1.0", "https://pel.hu/down/eos5dsr-v110r-win.zip"),
        ("1.1.1", "https://pel.hu/down/eos5dsr-v111r-win.zip"),
        ("1.1.2", "https://pel.hu/down/eos5dsr-v112r-win.zip"),
        ("1.1.3", "https://pel.hu/down/eos5dsr-v113r-win.zip"),
    ]),
    ("EOS_5D_MkIV", [
        ("1.0.2", "https://pel.hu/down/eos5d4-v102-win.zip"),
        ("1.0.3", "https://pel.hu/down/eos5d4-v103-win.zip"),
        ("1.0.4", "https://pel.hu/down/eos5d4-v104-win.zip"),
        ("1.1.2", "https://pel.hu/down/eos5d4-v112-win.zip"),
        ("1.2.1", "https://pel.hu/down/eos5d4-v121-win.zip"),
        ("1.3.0", "https://pel.hu/down/eos5d4-v130-win.zip"),
        ("1.3.1", "https://pel.hu/down/eos5d4-v131-win.zip"),
        ("1.3.2", "https://pel.hu/down/eos5d4-v132-win.zip"),
        ("1.3.3", "https://pel.hu/down/eos5d4-v133-win.zip"),
    ]),
    ("EOS_5D_MkIII", [
        ("1.1.2", "https://pel.hu/down/eos5d3-v112-win.zip"),
        ("1.1.3", "https://pel.hu/down/eos5d3-v113-win.zip"),
        ("1.2.1", "https://pel.hu/down/eos5d3-v121-win.zip"),
        ("1.2.3", "https://pel.hu/down/eos5d3-v123-win.zip"),
        ("1.3.3", "https://pel.hu/down/eos5d3-v133-win.zip"),
        ("1.3.4", "https://pel.hu/down/eos5d3-v134-win.zip"),
        ("1.3.5", "https://pel.hu/down/eos5d3-v135-win.zip"),
        ("1.3.6", "https://pel.hu/down/eos5d3-v136-win.zip"),
    ]),
    ("EOS_5D_MkII", [
        ("1.0.7", "https://pel.hu/down/eos5d2107.exe"),
        ("1.1.0", "https://pel.hu/down/eos5d2110.exe"),
        ("1.2.4", "https://pel.hu/down/eos5d2124.exe"),
        ("2.0.3", "https://pel.hu/down/eos5d2203.exe"),
        ("2.0.4", "https://pel.hu/down/eos5d2204.exe"),
        ("2.0.7", "https://pel.hu/down/eos5d2207.exe"),
        ("2.0.8", "https://pel.hu/down/eos5d2208.exe"),
        ("2.0.9", "https://pel.hu/down/eos5d2-v209-win.exe"),
        ("2.1.1", "https://pel.hu/down/5d2-v211-win.zip"),
        ("2.1.2", "https://pel.hu/down/eos5d2-v212-win.zip"),
    ]),
    ("EOS_5D", [
        ("1.0.5", "https://pel.hu/down/eos5d105.exe"),
        ("1.1.0", "https://pel.hu/down/eos5d110.exe"),
        ("1.1.1", "https://pel.hu/down/eos5d111.exe"),
    ]),
    ("EOS_6D_MkII", [
        ("1.0.3", "https://pel.hu/down/eos6d2-v103-win.zip"),
        ("1.0.4", "https://pel.hu/down/eos6d2-v104-win.zip"),
        ("1.0.5", "https://pel.hu/down/eos6d2-v105-win.zip"),
        ("1.1.0", "https://pel.hu/down/eos6d2-v110-win.zip"),
        ("1.1.1", "https://pel.hu/down/eos6d2-v111-win.zip"),
    ]),
    ("EOS_6D", [
        ("1.1.3", "https://pel.hu/down/eos6d-v113-win.zip"),
        ("1.1.4", "https://pel.hu/down/eos6d-v114-win.zip"),
        ("1.1.6", "https://pel.hu/down/eos6d-v116-win.zip"),
        ("1.1.7", "https://pel.hu/down/eos6d-v117-win.zip"),
        ("1.1.8", "https://pel.hu/down/eos6d-v118-win.zip"),
        ("1.1.9", "https://pel.hu/down/eos6d-v119-win.zip"),
    ]),
    ("EOS_7D_MkII", [
        ("1.0.4", "https://pel.hu/down/eos7d2-v104-win.zip"),
        ("1.0.5", "https://pel.hu/down/eos7d2-v105-win.zip"),
        ("1.1.0", "https://pel.hu/down/eos7d2-v110-win.zip"),
        ("1.1.1", "https://pel.hu/down/eos7d2-v111-win.zip"),
        ("1.1.2", "https://pel.hu/down/eos7d2-v112-win.zip"),
        ("1.1.3", "https://pel.hu/down/eos7d2-v113-win.zip"),
    ]),
    ("EOS_7D", [
        ("1.0.9", "https://pel.hu/down/eos7d109.exe"),
        ("1.1.0", "https://pel.hu/down/eos7d110.exe"),
        ("1.2.1", "https://pel.hu/down/eos7d121.exe"),
        ("1.2.2", "https://pel.hu/down/eos7d122.exe"),
        ("1.2.3", "https://pel.hu/down/eos7d123.exe"),
        ("1.2.5", "https://pel.hu/down/eos7d-v125.exe"),
        ("2.0.0", "https://pel.hu/down/eos7d-v200-win.zip"),
        ("2.0.3", "https://pel.hu/down/eos7d-v203-win.zip"),
        ("2.0.5", "https://pel.hu/down/eos7d-v205-win.zip"),
        ("2.0.6", "https://pel.hu/down/eos7d-v206-win.zip"),
    ]),
    ("EOS_90D", [
        ("1.1.1", "https://pel.hu/down/eos90d-v111-win.zip"),
    ]),
    ("EOS_80D", [
        ("1.0.2", "https://pel.hu/down/eos80d-v102-win.zip"),
        ("1.0.3", "https://pel.hu/down/eos80d-v103-win.zip"),
    ]),
    ("EOS_77D", [
        ("1.0.3", "https://pel.hu/down/v103-77d-9000d-win.zip"),
        ("1.1.0", "https://pel.hu/down/v110-77d-9000d-win.zip"),
    ]),
    ("EOS_70D", [
        ("1.1.2", "https://pel.hu/down/eos70d-v112-win.zip"),
        ("1.1.3", "https://pel.hu/down/eos70d-v113-win.zip"),
    ]),
    ("EOS_60D", [
        ("1.0.8", "https://pel.hu/down/eos60d108.exe"),
        ("1.0.9", "https://pel.hu/down/eos60d-v109.exe"),
        ("1.1.0", "https://pel.hu/down/eos60d-v110-win.zip"),
        ("1.1.1", "https://pel.hu/down/eos60d-v111-win.zip"),
        ("1.1.2", "https://pel.hu/down/eos60d-v112-win.zip"),
    ]),
    ("EOS_50D", [
        ("1.0.3", "https://pel.hu/down/eos50d103.exe"),
        ("1.0.6", "https://pel.hu/down/eos50d106.exe"),
        ("1.0.7", "https://pel.hu/down/eos50d107.exe"),
        ("1.0.8", "https://pel.hu/down/eos50d-v108-win.exe"),
        ("1.0.9", "https://pel.hu/down/eos50d-v109-win.zip"),
    ]),
    ("EOS_40D", [
        ("1.0.8", "https://pel.hu/down/eos40d108.exe"),
        ("1.1.1", "https://pel.hu/down/eos40d111.exe"),
    ]),
    ("EOS_30D", [
        ("1.0.4", "https://pel.hu/down/eos30d104.exe"),
        ("1.0.5", "https://pel.hu/down/eos30d105.exe"),
        ("1.0.6", "https://pel.hu/down/eos30d106.exe"),
    ]),
    ("EOS_20D", [
        ("2.0.2", "https://pel.hu/down/eos20d202.exe"),
        ("2.0.3", "https://pel.hu/down/eos20d203.exe"),
    ]),
    ("EOS_850D_T8i", []),  # fw not yet released
    ("EOS_800D_T7i", [
        ("1.0.2", "https://pel.hu/down/v102-t7i-800d-x9i-win.zip"),
        ("1.1.0", "https://pel.hu/down/v110-t7i-800d-x9i-win.zip"),
    ]),
    ("EOS_760D_T6s", [
        ("1.0.1", "https://pel.hu/down/v101-t6s-8000d-760d-win.zip"),
    ]),
    ("EOS_750D_T6i", [
        ("1.0.1", "https://pel.hu/down/v101-t6i-750d-x8i-win.zip"),
        ("1.1.0", "https://pel.hu/down/v110-t6i-750d-x8i-win.zip"),
    ]),
    ("EOS_700D_T5i", [
        ("1.1.3", "https://pel.hu/down/v113-t5i-700d-x7i-win.zip"),
        ("1.1.4", "https://pel.hu/down/v114-t5i-700d-x7i-win.zip"),
        ("1.1.5", "https://pel.hu/down/v115-t5i-700d-x7i-win.zip"),
    ]),
    ("EOS_650D_T4i", [
        ("1.0.4", "https://pel.hu/down/v104-t4i-650d-x6i-win.zip"),
        ("1.0.5", "https://pel.hu/down/v105-t4i-650d-x6i-win.zip"),
    ]),
    ("EOS_600D_T3i", [
        ("1.0.1", "https://pel.hu/down/v101-t3i-600d-x5.exe"),
        ("1.0.2", "https://pel.hu/down/eos600d-v102.zip"),
        ("1.0.3", "https://pel.hu/down/v103-t3i-600d-x5-win.zip"),
    ]),
    ("EOS_550D_T2i", [
        ("1.0.6", "https://pel.hu/down/B2704280.FIR"),
        ("1.0.8", "https://pel.hu/down/e8kr7108.exe"),
        ("1.0.9", "https://pel.hu/down/e8kr7109.exe"),
        ("1.1.0", "https://pel.hu/down/v110-t2i-550-x4-win.zip"),
    ]),
    ("EOS_500D_T1i", [
        ("1.1.0", "https://pel.hu/down/e7kr6110.exe"),
        ("1.1.1", "https://pel.hu/down/v111-t1i-500d-x3-win.exe"),
        ("1.1.2", "https://pel.hu/down/v112-t1i-500D-x3-win.zip"),
    ]),
    ("EOS_4000D_T100", [
        ("1.0.1", "https://pel.hu/down/v101-t100-3000d-4000d-win.zip"),
    ]),
    ("EOS_2000D_T7", [
        ("1.0.1", "https://pel.hu/down/v101-t7-2000d-1500d-x90-win.zip"),
        ("1.1.0", "https://pel.hu/down/v110-t7-2000d-1500d-x90-win.zip"),
    ]),
    ("EOS_1300D_T6", [
        ("1.0.2", "https://pel.hu/down/v102-t6-1300d-x80-win.zip"),
        ("1.1.0", "https://pel.hu/down/v110-t6-1300d-x80-win.zip"),
        ("1.1.1", "https://pel.hu/down/v111-t6-1300d-x80-win.zip"),
        ("1.2.0", "https://pel.hu/down/v120-t6-1300d-x80-win.zip"),
    ]),
    ("EOS_1200D_T5", [
        ("1.0.1", "https://pel.hu/down/v101-t5-1200d-x70-win.zip"),
        ("1.0.2", "https://pel.hu/down/v102-t5-1200d-x70-win.zip"),
    ]),
    ("EOS_1100D_T3", [
        ("1.0.5", "https://pel.hu/down/eos1100d-v105.zip"),
        ("1.0.6", "https://pel.hu/down/v106-t3-1100d-x50-win.zip"),
    ]),
    ("EOS_1000D_XS", [
        ("1.0.5", "https://pel.hu/down/e6kr5105.exe"),
        ("1.0.6", "https://pel.hu/down/e6kr5106.exe"),
        ("1.0.7", "https://pel.hu/down/e6kr5107.exe"),
    ]),
    ("EOS_450D_XSi", [
        ("1.0.9", "https://pel.hu/down/e5kr4109.exe"),
        ("1.1.0", "https://pel.hu/down/e5kr4110.exe"),
    ]),
    ("EOS_250D_SL3", [
        ("1.0.1", "https://pel.hu/down/v101-sl3-250d-200d2-x10-win.zip"),
        ("1.0.2", "https://pel.hu/down/v102-sl3-250d-200d2-x10-win.zip"),
    ]),
    ("EOS_200D_SL2", [
        ("1.0.1", "https://pel.hu/down/v101-sl2-200d-x9-win.zip"),
        ("1.0.3", "https://pel.hu/down/v103-sl2-200d-x9-win.zip"),
    ]),
    ("EOS_100D_SL1", [
        ("1.0.1", "https://pel.hu/down/v101-sl1-100d-x7-win.zip"),
    ]),
    ("EOS_R1", [
        ("1.0.1", "https://pel.hu/down/eosr1-v101-win.zip"),
        ("1.0.2", "https://pel.hu/down/eosr1-v102-win.zip"),
        ("1.1.2", "https://pel.hu/down/eosr1-v112-win.zip"),
        ("1.2.0", "https://pel.hu/down/eosr1-v120-win.zip"),
    ]),
    ("EOS_R3", [
        ("1.1.1", "https://pel.hu/down/eosr3-v111-win.zip"),
        ("1.2.0", "https://pel.hu/down/eosr3-v120-win.zip"),
        ("1.2.1", "https://pel.hu/down/eosr3-v121-win.zip"),
        ("1.2.2", "https://pel.hu/down/eosr3-v122-win.zip"),
        ("1.3.0", "https://pel.hu/down/eosr3-v130-win.zip"),
        ("1.4.0", "https://pel.hu/down/eosr3-v140-win.zip"),
        ("1.4.1", "https://pel.hu/down/eosr3-v141-win.zip"),
        ("1.5.1", "https://pel.hu/down/eosr3-v151-win.zip"),
        ("1.6.0", "https://pel.hu/down/eosr3-v160-win.zip"),
        ("1.7.1", "https://pel.hu/down/eosr3-v171-win.zip"),
        ("1.8.0", "https://pel.hu/down/eosr3-v180-win.zip"),
        ("1.9.0", "https://pel.hu/down/eosr3-v190-win.zip"),
        ("2.0.0", "https://pel.hu/down/eosr3-v200-win.zip"),
    ]),
    ("EOS_R5_MkII", [
        ("1.0.1", "https://pel.hu/down/eosr52-v101-win.zip"),
        ("1.0.2", "https://pel.hu/down/eosr52-v102-win.zip"),
        ("1.0.3", "https://pel.hu/down/eosr52-v103-win.zip"),
        ("1.1.1", "https://pel.hu/down/eosr52-v111-win.zip"),
        ("1.2.0", "https://pel.hu/down/eosr52-v120-win.zip"),
    ]),
    ("EOS_R5", [
        ("1.1.0", "https://pel.hu/down/eosr5-v110-win.zip"),
        ("1.1.1", "https://pel.hu/down/eosr5-v111-win.zip"),
        ("1.2.0", "https://pel.hu/down/eosr5-v120-win.zip"),
        ("1.3.0", "https://pel.hu/down/eosr5-v130-win.zip"),
        ("1.3.1", "https://pel.hu/down/eosr5-v131-win.zip"),
        ("1.4.0", "https://pel.hu/down/eosr5-v140-win.zip"),
        ("1.5.0", "https://pel.hu/down/eosr5-v150-win.zip"),
        ("1.5.1", "https://pel.hu/down/eosr5-v151-win.zip"),
        ("1.5.2", "https://pel.hu/down/eosr5-v152-win.zip"),
        ("1.6.0", "https://pel.hu/down/eosr5-v160-win.zip"),
        ("1.7.0", "https://pel.hu/down/eosr5-v170-win.zip"),
        ("1.8.1", "https://pel.hu/down/eosr5-v181-win.zip"),
        ("1.9.0", "https://pel.hu/down/eosr5-v190-win.zip"),
        ("2.0.0", "https://pel.hu/down/eosr5-v200-win.zip"),
        ("2.1.0", "https://pel.hu/down/eosr5-v210-win.zip"),
        ("2.2.0", "https://pel.hu/down/eosr5-v220-win.zip"),
        ("2.2.1", "https://pel.hu/down/eosr5-v221-win.zip"),
    ]),
    ("EOS_R6_MkIII", [
        ("1.0.1", "https://pel.hu/down/eosr63-v101-win.zip"),
        ("1.0.2", "https://pel.hu/down/eosr63-v102-win.zip"),
    ]),
    ("EOS_R6_MkII", [
        ("1.1.1", "https://pel.hu/down/eosr62-v111-win.zip"),
        ("1.1.2", "https://pel.hu/down/eosr62-v112-win.zip"),
        ("1.2.0", "https://pel.hu/down/eosr62-v120-win.zip"),
        ("1.3.0", "https://pel.hu/down/eosr62-v130-win.zip"),
        ("1.4.0", "https://pel.hu/down/eosr62-v140-win.zip"),
        ("1.5.0", "https://pel.hu/down/eosr62-v150-win.zip"),
        ("1.6.0", "https://pel.hu/down/eosr62-v160-win.zip"),
    ]),
    ("EOS_R6", [
        ("1.1.1", "https://pel.hu/down/eosr6-v111-win.zip"),
        ("1.2.0", "https://pel.hu/down/eosr6-v120-win.zip"),
        ("1.3.0", "https://pel.hu/down/eosr6-v130-win.zip"),
        ("1.3.1", "https://pel.hu/down/eosr6-v131-win.zip"),
        ("1.4.0", "https://pel.hu/down/eosr6-v140-win.zip"),
        ("1.5.0", "https://pel.hu/down/eosr6-v150-win.zip"),
        ("1.5.1", "https://pel.hu/down/eosr6-v151-win.zip"),
        ("1.5.2", "https://pel.hu/down/eosr6-v152-win.zip"),
        ("1.6.0", "https://pel.hu/down/eosr6-v160-win.zip"),
        ("1.7.0", "https://pel.hu/down/eosr6-v170-win.zip"),
        ("1.8.0", "https://pel.hu/down/eosr6-v180-win.zip"),
        ("1.8.1", "https://pel.hu/down/eosr6-v181-win.zip"),
        ("1.8.2", "https://pel.hu/down/eosr6-v182-win.zip"),
        ("1.8.3", "https://pel.hu/down/eosr6-v183-win.zip"),
        ("1.8.4", "https://pel.hu/down/eosr6-v184-win.zip"),
        ("1.9.0", "https://pel.hu/down/eosr6-v190-win.zip"),
    ]),
    ("EOS_R8", [
        ("1.1.0", "https://pel.hu/down/eosr8-v110-win.zip"),
        ("1.2.0", "https://pel.hu/down/eosr8-v120-win.zip"),
        ("1.3.0", "https://pel.hu/down/eosr8-v130-win.zip"),
        ("1.4.0", "https://pel.hu/down/eosr8-v140-win.zip"),
    ]),
    ("EOS_R", [
        ("1.1.0", "https://pel.hu/down/eosr-v110-win.zip"),
        ("1.2.0", "https://pel.hu/down/eosr-v120-win.zip"),
        ("1.3.0", "https://pel.hu/down/eosr-v130-win.zip"),
        ("1.4.0", "https://pel.hu/down/eosr-v140-win.zip"),
        ("1.6.0", "https://pel.hu/down/eosr-v160-win.zip"),
        ("1.7.0", "https://pel.hu/down/eosr-v170-win.zip"),
        ("1.8.0", "https://pel.hu/down/eosr-v180-win.zip"),
        ("1.8.0v2", "https://pel.hu/down/eosr-v180-win2.zip"),
    ]),
    ("EOS_RP", [
        ("1.1.0", "https://pel.hu/down/eosrp-v110-win.zip"),
        ("1.2.0", "https://pel.hu/down/eosrp-v120-win.zip"),
        ("1.3.0", "https://pel.hu/down/eosrp-v130-win.zip"),
        ("1.4.0", "https://pel.hu/down/eosrp-v140-win.zip"),
        ("1.5.0", "https://pel.hu/down/eosrp-v150-win.zip"),
        ("1.6.0", "https://pel.hu/down/eosrp-v160-win.zip"),
    ]),
    ("EOS_R7", [
        ("1.1.0", "https://pel.hu/down/eosr7-v110-win.zip"),
        ("1.2.0", "https://pel.hu/down/eosr7-v120-win.zip"),
        ("1.3.0", "https://pel.hu/down/eosr7-v130-win.zip"),
        ("1.3.1", "https://pel.hu/down/eosr7-v131-win.zip"),
        ("1.4.0", "https://pel.hu/down/eosr7-v140-win.zip"),
        ("1.5.0", "https://pel.hu/down/eosr7-v150-win.zip"),
    ]),
    ("EOS_R10", [
        ("1.1.0", "https://pel.hu/down/eosr10-v110-win.zip"),
        ("1.2.0", "https://pel.hu/down/eosr10-v120-win.zip"),
        ("1.3.0", "https://pel.hu/down/eosr10-v130-win.zip"),
        ("1.4.0", "https://pel.hu/down/eosr10-v140-win.zip"),
        ("1.5.0", "https://pel.hu/down/eosr10-v150-win.zip"),
        ("1.6.0", "https://pel.hu/down/eosr10-v160-win.zip"),
    ]),
    ("EOS_R50", [
        ("1.1.0", "https://pel.hu/down/eosr50-v110-win.zip"),
        ("1.2.0", "https://pel.hu/down/eosr50-v120-win.zip"),
        ("1.3.0", "https://pel.hu/down/eosr50-v130-win.zip"),
    ]),
    ("EOS_R50V", [
        ("1.1.1", "https://pel.hu/down/eosr50v-v111-win.zip"),
    ]),
    ("EOS_R100", [
        ("1.1.0", "https://pel.hu/down/eosr100-v110-win.zip"),
    ]),
    ("EOS_M200", []),  # fw not yet released
    ("EOS_M100", [
        ("1.0.1", "https://pel.hu/down/eosm100-v101-win.zip"),
    ]),
    ("EOS_M50_MkII", [
        ("1.0.1", "https://pel.hu/down/M502-KissM2-v101-win.zip"),
        ("1.0.3", "https://pel.hu/down/M502-KissM2-v103-win.zip"),
    ]),
    ("EOS_M50", [
        ("1.0.1", "https://pel.hu/down/M50-KissM-v101-win.zip"),
        ("1.0.2", "https://pel.hu/down/M50-KissM-v102-win.zip"),
        ("1.0.3", "https://pel.hu/down/M50-KissM-v103-win.zip"),
    ]),
    ("EOS_M10", [
        ("1.1.0", "https://pel.hu/down/eosm10-v110-win.zip"),
        ("1.1.1", "https://pel.hu/down/eosm10-v111-win.zip"),
    ]),
    ("EOS_M6_MkII", [
        ("1.0.1", "https://pel.hu/down/eosm6m2-v101-win.zip"),
        ("1.1.0", "https://pel.hu/down/eosm6m2-v110-win.zip"),
        ("1.1.1", "https://pel.hu/down/eosm6m2-v111-win.zip"),
    ]),
    ("EOS_M6", [
        ("1.0.1", "https://pel.hu/down/eosm6-v101-win.zip"),
    ]),
    ("EOS_M5", [
        ("1.0.1", "https://pel.hu/down/eosm5-v101-win.zip"),
        ("1.0.2", "https://pel.hu/down/eosm5-v102-win.zip"),
    ]),
    ("EOS_M3", [
        ("1.1.0", "https://pel.hu/down/eosm3-v110-win.zip"),
        ("1.2.0", "https://pel.hu/down/eosm3-v120-win.zip"),
        ("1.2.1", "https://pel.hu/down/eosm3-v121-win.zip"),
    ]),
    ("EOS_M2", [
        ("1.0.3", "https://pel.hu/down/eosm2-v103-win.zip"),
        ("1.0.4", "https://pel.hu/down/eosm2-v104-win.zip"),
    ]),
    ("EOS_M", [
        ("2.0.2", "https://pel.hu/down/eosm-v202-win.zip"),
        ("2.0.3", "https://pel.hu/down/eosm-v203-win.zip"),
    ]),
]


def download_file(url, dest_path, retries=3):
    """Download a file with retries and rate limiting."""
    for attempt in range(retries):
        try:
            print(f"  Downloading: {os.path.basename(dest_path)} ({attempt+1}/{retries})")
            req = urllib.request.Request(url, headers={
                'User-Agent': 'Mozilla/5.0 (compatible; ML70D)'
            })
            with urllib.request.urlopen(req, timeout=120) as response:
                data = response.read()
                with open(dest_path, 'wb') as f:
                    f.write(data)
            size_mb = len(data) / (1024 * 1024)
            print(f"  OK: {os.path.basename(dest_path)} ({size_mb:.1f} MB)")
            return True
        except urllib.error.HTTPError as e:
            print(f"  HTTP {e.code}: {url}")
            return False
        except Exception as e:
            if attempt < retries - 1:
                wait = 5 * (attempt + 1)
                print(f"  Error: {e}, retrying in {wait}s...")
                time.sleep(wait)
            else:
                print(f"  FAILED: {e}")
                return False


def main():
    os.makedirs(FIRMWARE_DIR, exist_ok=True)

    total_files = sum(len(fws) for _, fws in CAMERAS)
    print(f"Found {len(CAMERAS)} camera models, {total_files} firmware files total\n")

    downloaded = 0
    failed = 0
    skipped = 0

    for cam_name, firmwares in CAMERAS:
        cam_dir = os.path.join(FIRMWARE_DIR, cam_name)
        os.makedirs(cam_dir, exist_ok=True)

        if not firmwares:
            print(f"[{cam_name}] No firmware links (not yet released)")
            continue

        for version, url in firmwares:
            fname = os.path.basename(url)
            dest = os.path.join(cam_dir, fname)

            if os.path.exists(dest) and os.path.getsize(dest) > 0:
                print(f"[{cam_name}] {fname} already exists, skipping")
                skipped += 1
                continue

            print(f"[{cam_name}] v{version}: ", end="")
            if download_file(url, dest):
                downloaded += 1
            else:
                failed += 1

            time.sleep(2)  # rate limit - don't hammer the server

    print(f"\nDone: {downloaded} downloaded, {skipped} skipped, {failed} failed")

if __name__ == "__main__":
    main()
