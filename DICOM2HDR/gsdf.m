% vectorize 1-27-2019
function ret = gsdf (j)

a = -1.3011877;
b = -2.584019E-2;
c = 8.0242636E-2;
d = -1.0320229E-1;
e = 1.3646699E-1;
f = 2.8745620E-2;
g = -2.5468404E-2;
h = -3.1978977E-3;
k = 1.2992634E-4;
m = 1.3635334E-3;

logj = log(j);
r = ( a + c * logj + e * logj.^2 + g * logj.^3 + m * logj.^4)...
    ./(1 + b * logj + d * logj.^2 + f * logj.^3 + h * logj.^4 + k * logj.^5);

ret = power(10,r);
end
