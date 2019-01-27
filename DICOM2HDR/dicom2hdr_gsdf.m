% 1-27-2019
% Q: Calibrate a DICOM image for an HDR TV

function dicom2hdr_gsdf (fn)

fn_in = [fn '.dcm'];
fn_out = [fn '_gsdf.hdr'];

% luminance measurement data of TCL 65S513
% lumdata(:,1) is input value, HDR pixel, [0,12.5]
% lumdata(:,2) is measured luminance by Minolta CS100
load('luminance','lumdata')

% the luminance got saturated after the 33 entry
lumdata_unique = lumdata(1:32,:);

% GSDF calibration
Lmin = min(lumdata_unique(:,2))
Lmax = max(lumdata_unique(:,2))

% get the JND_index
Jmin = gsdfinv(Lmin)
Jmax = gsdfinv(Lmax)

% normalize the DICOM pixels to [0,1]
X = double(dicomread(fn_in))./4096;

% convert pixel into JND_index
% notice that interp1 returns NaN if x is out of input range
J = interp1([0 1],[Jmin Jmax],X);

% convert JND_index into luminance; notice that gsdf needs to process
% vectors
Lgamma = gsdf(J);

% truncate into [Lmin,Lmax]; otherwise got NaN later in interp1
Lgamma = max(Lgamma, Lmin);
Lgamma = min(Lgamma, Lmax);

% look up the luminance measurement table
% notice that interp1 returns NaN if x is out of input range
Lcode = interp1(lumdata_unique(:,2),lumdata_unique(:,1),Lgamma);

% convert monochrome into color image
XX3 = repmat(Lcode,1,1,3);

% create the HDR file
hdrwrite(XX3,fn_out)

end

