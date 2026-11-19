/ 808 Rimshot — 1800Hz dominant, dead in 20ms
N: ms 60
T: !N
/ very fast overall decay
E: e(T*(0-6.9%N))
/ 1800Hz tone — the dominant frequency
F: hz 1800
P: +\(N#F)
S: (s P)
/ broadband noise crack
R: r T
L: 0.15 f R
H: R-L
W: w E*(S*.6+H*.5)

O: 1
W
