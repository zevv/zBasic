
10 cls() : zoom = 13 : max = 15 : min = 15 : dx = -0.235125 : dy = -0.827215
20 for nn = 1 to 80
30 for i = -15 to 15
40 for j = -20 to 20
50 x = j/zoom+dx : y = i/zoom+dy
60 gosub 1000
70 next
80 next
90 zoom = zoom * 1.05
100 if min > 1 then max = max * 1.1
110 min = 15 : next

1000 a = 0 : b = 0 : n = 0
1010 aa = a*a : bb = b*b
1020 b = 2*a*b+y : a = aa-bb+x
1040 n = n + 1
1050 if n<max and aa+bb<4 then goto 1010
1060 color = 16*n/max
1070 plot(j+20, i+15, color)
1080 if color < min then min = color
1090 return

run
