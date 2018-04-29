
10 cls :  a = 0
20 gosub 100
30 a = a + 0.125
40 sleep 1/60
50 goto 20

100 for x = -15 to 15
105 for y = -15 to 15
110 color = (sqrt(x*x+y*y)/4+a) % 16
115 plot x+15, y+15
120 next
125 next
130 return

run

