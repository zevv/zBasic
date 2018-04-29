
1 cls : color = 1 : zoom = 13 : max = 40 : dx = -0.5 : dy = 0
2 for i = -15 to 15
3 for j = -20 to 20
4 x = j/zoom+dx : y = i/zoom+dy
5 gosub 10
6 next
7 next
8 end

10 a = 0 : b = 0 : n = 0
11 ta = a*a-b*b + x
12 b = 2*a*b + y
13 a = ta
14 n = n + 1
15 if n<max and a*a+b*b<4 then goto 11
16 color = 15 * n/max
17 plot j+20, i+15
18 return

run
