
5 print "Hallo"
10 cls() :  a = 1.3
20 gosub 100
30 a = a + 0.125
50 goto 20

100 for x = -15 to 15
105 for y = -15 to 15
110 color = ((x*x+y*y)/12+a) % 16
115 plot(x+15, y+15, color)
120 next
125 next
130 return

run

