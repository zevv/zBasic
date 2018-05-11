  1 errors = 0
  2 l =  2 : if 1+4*4 != 17 then gosub 1000
  3 l =  3 : if 4*4+1 != 17 then gosub 1000
  4 l =  4 : if 3**3**3 != 7625597484987 then gosub 1000
  5 l =  5 : if (3**3)**3 != 19683 then gosub 1000
  6 l =  6 : if 3**(3**3) != 7625597517824 then gosub 1000
  7 l =  7 : n = 0 : for i = 1 to 3 : for j = 1 to 3 : n = n + 1 : next : next : if n != 9 then gosub 1000
  8 l =  8 : n = 0 : for i = 1 to 3 : for j = i to 3 : n = n + 1 : next : next : if n != 6 then gosub 1000
  9 l =  9 : n = 0 : for i = 1 to 3 : for j = 1 to i : n = n + 1 : next : next : if n != 6 then gosub 1000
 10 l = 10 : if (1) != 1 then gosub 1000
 11 l = 11 : if (1+1) != 2 then gosub 1000
 12 l = 12 : if (1+1+1) != 3 then gosub 1000
 13 l = 13 : if 1+2*3 != 7 then gosub 1000
 14 l = 14 : if (1+2)*3 != 9 then gosub 1000
 15 l = 15 : if (1+2)*3 != 3*(1+2) then gosub 1000
 16 l = 16 : if 1-5 != -4 then gosub 1000
 17 l = 17 : if -1+-1 != -2 then gosub 1000
 18 l = 18 : if -1 > 1 then gosub 1000
 19 l = 19 : if ((20-10)*(30-20)+10)*2 != 220 then gosub 1000
 20 l = 20 : if 1*-3 != -3 then gosub 1000
 21 l = 21 : a = b = 1 + 2 : if a != 3 or b !=3 then gosub 1000
 22 l = 22 : if -3**2 != -9 then gosub 1000
 23 l = 23 : if (-3)**2 != 9 then gosub 1000
 24 l = 24 : if ------2 != 2 then gosub 1000
 25 l = 25 : if 2**8 != 256 then gosub 1000
 26 l = 26 : if 2**9 != 512 then gosub 1000
 27 l = 27 : if 2**10 != 1024 then gosub 1000
 28 l = 28 : if 2**11 != 2048 then gosub 1000
 29 l = 29 : if 2**12 != 4096 then gosub 1000
 30 l = 30 : if 2**13 != 8192 then gosub 1000
 31 l = 31 : if 2**14 != 16384 then gosub 1000
 32 l = 32 : if 2**15 != 32768 then gosub 1000
 33 l = 33 : if 2**16 != 65536 then gosub 1000
 34 l = 34 : if 1.5*2 != 3 then gosub 1000
 35 l = 35 : if 2*2*2*2*2*2*2*2*127 != 32512 then gosub 1000
 36 l = 36 : if 13*41*61 != 32513 then gosub 1000
 37 l = 37 : a = 0 : for i = 1 to 100 step 3 : a = a + 1 : next : if a != 34 then gosub 1000
 38 l = 38 : a = 0 : for i = 100 to 1 step -3 : a = a + 1 : next : if a != 34 then gosub 1000
 39 l = 39 : a = 5 : A = 10 : b = a + A : if b != 15 then gosub 1000
 40 l = 40 : if 1/8 != 0.125 then gosub 1000
 41 l = 41 : if 0x20 != 32 then gosub 1000
 42 l = 42 : if (0x55 ^ 0xff) != 170 then gosub 1000
 43 l = 43 : if (0x10 | 0x20) != 0x30 then gosub 1000
 44 l = 44 : if (85 & 255) != 85 then gosub 1000
 45 l = 45 : if !0 != 1 then gosub 1000
 46 l = 46 : if !1 != 0 then gosub 1000
 47 l = 47 : if !!1 != 1 then gosub 1000
 48 l = 48 : if !!!1 != 0 then gosub 1000
 49 l = 49 : if !3 != 0 then gosub 1000
 50 l = 50 : if ~0 != -1 then gosub 1000
 51 l = 51 : if (255 & (~32)) != 223 then gosub 1000
 52 l = 52 : if (1 or 1 and 0) != 1 then gosub 1000
 53 l = 53 : if 32 >> 1 != 16 then gosub 1000
 54 l = 54 : if 32 >> 6 != 0 then gosub 1000
 55 l = 55 : if (1<< 16) != 65536 then gosub 1000
 56 l = 56 : for i = 1 to 16 : if (1<<i) != (2**i) then gosub 1000 : next


900 if errors != 0 then exit(1) else print "ok"
999 end

1000 print "Fail test " ; l
1010 errors = errors + 1
1020 return


run
