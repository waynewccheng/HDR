load('data 0_35 no local dimming\luminance.mat')
d0c0 = luminance

load('data 0_35 local dimming\luminance.mat')
d1c0 = luminance

load('data 0_35 dynamic contrast\luminance.mat')
d0c1 = luminance

load('data 0_35 local dimming dynamic contrast\luminance')
d1c1 = luminance

range = 0:3.5/10:3.5

clf
hold on
plot(range,d0c0,'o-')
plot(range,d1c0,'o-')
plot(range,d0c1,'o-')
plot(range,d1c1,'o-')

legend('None','Local Dimming Only','Dynamic Contrast Only','Both')
xlabel('HDR Value')
ylabel('Luminance')
