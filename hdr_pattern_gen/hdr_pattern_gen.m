% 2-5-2019
% rename to hdr_pattern_gen

% 1-27-2019
% Generate HDR patterns for measuring
% gray patch on black (10% in size)
% lum is the value for .HDR image [0,12.5]

function hdr_pattern_gen (lum)

% size of full screen 4K
maxx = 3840;
maxy = 2160;

% center point
centerx = maxx/2;
centery = maxy/2;

% area percentage
perc = 0.1;

% filename
fn = filename_gen(lum,4)

% calculate sides
% example: 10% area
%  x * sqrt(0.1) * y * sqrt(0.1) = x * y * 0.1
sizex = round(maxx * (perc.^0.5))
sizey = round(maxy * (perc.^0.5))

% make it an even number
sizex = round(sizex / 2) * 2;
sizey = round(sizey / 2) * 2;


% canvas
% black background to reduce glare
im = zeros(maxy,maxx,3);

% paint the white patch
im(centery-sizey/2:centery+sizey/2,centerx-sizex/2:centerx+sizex/2,1) = lum;
im(centery-sizey/2:centery+sizey/2,centerx-sizex/2:centerx+sizex/2,2) = lum;
im(centery-sizey/2:centery+sizey/2,centerx-sizex/2:centerx+sizex/2,3) = lum;

% visualize
imagesc(im); colormap gray;
imwrite(im,[fn '.png']);

% create the HDR image file
% hdrwrite(im,[fn '.hdr']);

return


end
