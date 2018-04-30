
1 cls : color = 1 : zoom = 13 : max = 40 : dx = -1.48 : dy = 0 : min = 15
2 for i = -15 to 15
3 for j = -20 to 20
4 x = j/zoom+dx : y = i/zoom+dy
5 gosub 100
6 next
7 next
8 zoom = zoom * 1.1 
10 if min > 1.5 then max = max * 1.1
11 min = 15 : goto 2

100 a = 0 : b = 0 : n = 0
101 ta = a*a-b*b + x
102 b = 2*a*b + y
103 a = ta
104 n = n + 1
105 if n<max and a*a+b*b<4 then goto 101
106 color = 15 * n/max
107 plot j+20, i+15
108 if color < min then min = color
109 return

run
