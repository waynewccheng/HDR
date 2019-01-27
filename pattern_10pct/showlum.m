% 1-27-2019
% Generate HDR patterns for measuring
% gray patch on black (10% in size)
% lum is the value for .HDR image [0,12.5]

function showlum (lum)

% size of full screen 4K
maxx = 3840;
maxy = 2160;

% center point
centerx = maxx/2;
centery = maxy/2;

% tried 1/10 but too small
sizex = maxx/10;
sizey = maxy/10;

% use Rtings.com's 10% test pattern
sizex = 1218;
sizey = 684;

% canvas
im = zeros(maxy,maxx,3);

% paint the white patch
im(centery-sizey/2:centery+sizey/2,centerx-sizex/2:centerx+sizex/2,1) = lum;
im(centery-sizey/2:centery+sizey/2,centerx-sizex/2:centerx+sizex/2,2) = lum;
im(centery-sizey/2:centery+sizey/2,centerx-sizex/2:centerx+sizex/2,3) = lum;

% visualize
imagesc(im); colormap gray;
% imwrite(im,'test.png');

% create the HDR image file
fn = sprintf('%03d.hdr',round(lum*10))
hdrwrite(im,fn);

end
