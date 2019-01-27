% revised inverse 10-5-2016

function ret = gsdfinv (L)
A=71.498068;
B=94.593053;
C=41.912053;
D=9.8247004;
E=0.28175407;
F=-1.1878455;
G=-0.18014349;
H=0.14710899;
I=-0.017046845;

x = log10(L);
ret = A + B * x + C * x.^2 + D * x.^3 + E * x.^4 + F * x.^5 + G * x.^6 + H * x.^7 + I * x.^8;

end

function ret = gsdfinv_binary_search (j)
  tmin = 1;
  tmax = 1023;
  
  while 1
    ttry = (tmin + tmax)/2; 
    r = gsdf(ttry);
    if (r>j)
        tmax = ttry-1;
    else if (r<j)
        tmin = ttry+1;
            else
                ret = ttry;
                break;
        end
    end
        
    if (tmax<=tmin)
        ret = tmax;
        break;
    end
    
  end
end

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
r = ( a + c * logj + e * power(logj,2) + g * power(logj,3) + m * power(logj,4) )...
    /(1 + b * logj + d * power(logj,2) + f * power(logj,3) + h * power(logj,4) + k * power(logj,5));

ret = power(10,r);
end
