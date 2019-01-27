% Q: how to create an HDR image (.exr) for HDRDisplay
function dicom2hdr_gamma (fn)

fn_in = [fn '.dcm'];
fn_out = [fn '_gamma.hdr'];

load('luminance','lumdata')
lumdata_unique = lumdata(1:32,:);
Lmin = min(lumdata_unique(:,2))
Lmax = max(lumdata_unique(:,2))
Jmin = gsdfinv(Lmin)
Jmax = gsdfinv(Lmax)

x = 0:1/4095:1;
y = x.^2.2;
Lgamma = (Lmax-Lmin)*y + Lmin;
Lcode = interp1(lumdata_unique(:,2),lumdata_unique(:,1),Lgamma);

X = double(dicomread(fn_in))./4096;

XX = interp1(x,Lcode,X);

XX3 = repmat(XX,1,1,3);

hdrwrite(XX3,fn_out)

end

