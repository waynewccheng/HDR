% Q: how to create an HDR image (.exr) for HDRDisplay
function dicom2hdr_gsdf (fn)

fn_in = [fn '.dcm'];
fn_out = [fn '_gsdf.hdr'];

% luminance data from TCL
load('luminance','lumdata')
lumdata_unique = lumdata(1:32,:);


Lmin = min(lumdata_unique(:,2))
Lmax = max(lumdata_unique(:,2))
Jmin = gsdfinv(Lmin)
Jmax = gsdfinv(Lmax)

X = double(dicomread(fn_in))./4096;
J = interp1([0 1],[Jmin Jmax],X);

Lgamma = gsdf(J);

Lgamma = max(Lgamma, Lmin);
Lgamma = min(Lgamma, Lmax);

Lcode = interp1(lumdata_unique(:,2),lumdata_unique(:,1),Lgamma);

XX3 = repmat(Lcode,1,1,3);

hdrwrite(XX3,fn_out)

end

