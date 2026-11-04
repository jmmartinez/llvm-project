#!/bin/bash
source ~/llvm/sqtt-fuzz/env.sh && afl-whatsup -s ~/llvm/sqtt-fuzz/$1
