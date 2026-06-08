% =========================================================================
%  DelayProcessing.m  -  TDOA 3D lokalizacija izvora zvuka
%  STM32G474RE + MatlabHelper (3 Mbaud, ~32 kHz)
%
%  EVENT-TRIGGERED nacin rada:
%    STM32 sam detektira pljeskanje (energija po bloku) i salje SAMO prozor
%    oko dogadaja (EVENT_PRE + trigger + EVENT_POST = 17 blokova = 1088
%    uzoraka/kanalu ~ 34 ms). Izmedu pljeskanja je linija u mirovanju, pa
%    host nikad ne preplavi buffer -> detekcija je pouzdana.
%
%  Svaki primljeni paket = jedno pljeskanje. Po paketu:
%    1. Sprema mic1..mic4, tdoa_us, az_deg, el_deg u workspace
%    2. GCC-PHAT TDOA -> 3D DOA (azimut/elevacija)
%    3. Crta: 4 mic signala + 3 GCC-PHAT korelacije
%    4. Ispisuje kut u Command Window
% =========================================================================
clear; clc; close all;

%% =========================================================================
%  KONFIGURACIJA  -  mora odgovarati app_config.h na STM32 strani
%% =========================================================================
COM_PORT        = 'COM5';
BAUD_RATE       = 3000000;
NUM_CHANNELS    = 4;

% --- mora se poklapati s app_config.h ---
ADC_BLOCK_SAMPLES = 64;                        % ADC_BLOCK_SAMPLES
EVENT_PRE_BLOCKS  = 4;                          % EVENT_PRE_BLOCKS
EVENT_POST_BLOCKS = 12;                         % EVENT_POST_BLOCKS
EVENT_WIN_BLOCKS  = EVENT_PRE_BLOCKS + 1 + EVENT_POST_BLOCKS;   % = 17
SAMPLES_PER_CH    = EVENT_WIN_BLOCKS * ADC_BLOCK_SAMPLES;       % = 1088
FS                = 170e6 / 5312;               % 32 003.01 Hz (TIM3 ARR=5311)

% indeks (priblizan) trenutka okidanja unutar prozora
TRIG_SAMPLE       = EVENT_PRE_BLOCKS * ADC_BLOCK_SAMPLES + 1;   % ~ uzorak 257

C_SOUND         = 343.0;
HPF_CUTOFF_HZ   = 300;
LPF_CUTOFF_HZ   = 10000;

MIC_POS = [                      % prilagodi stvarnoj geometriji [m]
    0,       0,      0;          % MIC1  PB14/ADC1_IN5
    0.0867,  0.05,   0;          % MIC2  PC0/ADC1_IN6
    0.0867, -0.05,   0;          % MIC3  PC1/ADC1_IN7
    0.05,    0,      0.08        % MIC4  PC2/ADC1_IN8
];

% Soft sanity-check: STM32 je vec okinuo, ali odbacujemo paket ako je
% prakticki ravan (npr. lazni okid). Snizi/digni po potrebi.
MIN_EVENT_DB    = 6;     % vrh ovojnice mora biti barem ovoliko iznad medijana

%% =========================================================================
%  IZVEDENE VELICINE
%% =========================================================================
PAYLOAD_BYTES = NUM_CHANNELS * SAMPLES_PER_CH * 2;   % = 4*1088*2 = 8704 B

max_dist    = max_pair_dist(MIC_POS);
MAX_TDOA_US = max_dist / C_SOUND * 1e6;
MAX_LAG     = ceil(MAX_TDOA_US * 1e-6 * FS * 1.5);
LAG_US      = (-MAX_LAG:MAX_LAG) / FS * 1e6;

[b_bp, a_bp] = butter(3, [HPF_CUTOFF_HZ LPF_CUTOFF_HZ]/(FS/2), 'bandpass');
COLORS = lines(NUM_CHANNELS);

fprintf('Fs=%.0f Hz | Prozor=%d uzoraka (%.1f ms) | MaxTDOA=%.0f us | MaxLag=%d\n', ...
        FS, SAMPLES_PER_CH, SAMPLES_PER_CH/FS*1e3, MAX_TDOA_US, MAX_LAG);
fprintf('Paket: %.0f B  (~%.1f ms TX po pljesku)\n\n', ...
        PAYLOAD_BYTES+4, (PAYLOAD_BYTES+4)*10/BAUD_RATE*1e3);

%% =========================================================================
%  SERIJSKI PORT
%% =========================================================================
s = serialport(COM_PORT, BAUD_RATE, 'Timeout', 30.0);
s.ByteOrder = 'little-endian';
flush(s);
fprintf('Port %s otvoren. Cekam pljeskanje...\n\n', COM_PORT);

%% =========================================================================
%  INTERNE VARIJABLE
%% =========================================================================
ev_count = 0;
t0       = tic;
fig_h    = [];

%% =========================================================================
%  GLAVNA PETLJA  -  svaki paket je jedan detektirani dogadaj
%% =========================================================================
cleanup = onCleanup(@() safe_clear(s));
while true

    % --- citaj jedan event paket (blocking: sync + PAYLOAD_BYTES) ---
    payload = read_one_packet(s, PAYLOAD_BYTES);
    tnow    = toc(t0);

    % --- parsiraj: interleaved uint16 -> (SAMPLES_PER_CH x NUM_CHANNELS) ---
    u16   = typecast(uint8(payload(:)), 'uint16');
    x_raw = reshape(double(u16), NUM_CHANNELS, SAMPLES_PER_CH).';

    % --- DC removal + bandpass ---
    x   = x_raw - median(x_raw, 1);
    x_f = filtfilt(b_bp, a_bp, x);

    % --- energetska ovojnica (sanity check + lokacija vrha za prikaz) ---
    env_win = max(8, round(0.8e-3 * FS));
    env_db  = 10*log10(movmean(mean(x_f.^2, 2), env_win) + eps);
    md_db   = median(env_db);
    [pk_db, peak_samp] = max(env_db);

    if (pk_db - md_db) < MIN_EVENT_DB
        fprintf('[%+.1fs] paket primljen ali prakticki ravan (%.1f dB) - preskacem\n', ...
                tnow, pk_db - md_db);
        continue;
    end

    ev_count = ev_count + 1;

    % --- GCC-PHAT TDOA preko cijelog prozora ---
    % G = FFT(MIC1)*conj(FFT(MIC_i)): pozitivan lag = MIC_i prima KASNIJE = dalje od izvora
    tdoa_us_v = zeros(1,3);
    xc_all    = zeros(2*MAX_LAG+1, 3);
    for mi = 2:NUM_CHANNELS
        [xc, ~]         = gcc_phat(x_f(:,1), x_f(:,mi), MAX_LAG);
        [~, ix]         = max(xc);
        tdoa_us_v(mi-1) = LAG_US(ix);
        xc_all(:,mi-1)  = xc;
    end

    % --- 3D DOA:  A * d = c * tau ---
    A  = MIC_POS(1,:) - MIC_POS(2:end,:);
    u  = A \ (C_SOUND * tdoa_us_v(:) * 1e-6);
    un = norm(u);
    if un < 1e-9
        u_hat = [0;0;1]; az_deg = NaN; el_deg = NaN;
    else
        u_hat  = u / un;
        az_deg = atan2d(u_hat(2), u_hat(1));
        el_deg = asind(max(-1, min(1, u_hat(3))));
    end

    % --- spremi u workspace ---
    t_win_ms = (0:SAMPLES_PER_CH-1) / FS * 1e3;
    for mi = 1:NUM_CHANNELS
        assignin('base', sprintf('mic%d', mi),     x_f(:,mi));
        assignin('base', sprintf('mic%d_raw', mi), x(:,mi));
    end
    assignin('base','tdoa_us',  tdoa_us_v);
    assignin('base','az_deg',   az_deg);
    assignin('base','el_deg',   el_deg);
    assignin('base','u_hat',    u_hat);
    assignin('base','t_win_ms', t_win_ms);

    % --- ispis ---
    fprintf('[#%d | %+.1fs]  Az=%+.1f deg  El=%+.1f deg  TDOA=[%+.1f %+.1f %+.1f] us  (pk %.0f@%.1fms)\n', ...
            ev_count, tnow, az_deg, el_deg, tdoa_us_v(1), tdoa_us_v(2), tdoa_us_v(3), ...
            pk_db - md_db, (peak_samp-TRIG_SAMPLE)/FS*1e3);

    % --- crtaj ---
    fig_h = draw_event(fig_h, x_f, t_win_ms, xc_all, LAG_US, tdoa_us_v, ...
                       az_deg, el_deg, COLORS, MAX_TDOA_US, ev_count, tnow);
end

%% =========================================================================
%  CITANJE JEDNOG PAKETA  -  pronadi sync, zatim citaj payload
%  Izmedu pljeskanja read blokira (linija mirna) - nema opterecenja CPU-a.
%% =========================================================================
function payload = read_one_packet(s, payload_bytes)
win = uint8(zeros(1, 4));
while ~all(win == 255)
    b   = read(s, 1, 'uint8');
    win = [win(2:end), uint8(b)];
end
payload = read(s, payload_bytes, 'uint8');
end

%% =========================================================================
%  CRTANJE NA EVENT
%% =========================================================================
function fig = draw_event(fig, xw, t_ms, xc_all, lag_us, tdoa_us_v, ...
                          az_deg, el_deg, colors, max_tdoa_us, ev_no, tnow)
if isempty(fig) || ~ishandle(fig)
    fig = figure('Name','Sound Localization - Event', 'Color','w', ...
                 'Position',[100 80 1100 620]);
end
clf(fig);

tl = tiledlayout(fig, 2, 3, 'TileSpacing','compact', 'Padding','compact');
title(tl, sprintf('Detekcija #%d  |  t=%.1f s  |  Az=%+.1f deg  El=%+.1f deg', ...
      ev_no, tnow, az_deg, el_deg), 'FontSize',12, 'FontWeight','bold');

% --- gornji red (cijela sirina): 4 mic signala ---
ax_sig = nexttile(1, [1 3]);
hold(ax_sig,'on'); grid(ax_sig,'on');
for i = 1:size(xw,2)
    plot(ax_sig, t_ms, xw(:,i), '-', 'Color',colors(i,:), 'LineWidth',1.3, ...
         'DisplayName', sprintf('MIC%d',i));
end
legend(ax_sig,'Location','northeast','FontSize',8);
xlabel(ax_sig,'Vrijeme [ms]');  ylabel(ax_sig,'ADC (BP filtrirano)');
title(ax_sig, sprintf('Prozor oko pljeskanja  (%.0f ms)', t_ms(end)-t_ms(1)));
xlim(ax_sig,[t_ms(1) t_ms(end)]);

% --- donji red: 3 GCC-PHAT korelacije ---
pair_names = {'MIC1 <-> MIC2','MIC1 <-> MIC3','MIC1 <-> MIC4'};
for i = 1:3
    ax = nexttile(3+i);
    hold(ax,'on'); grid(ax,'on');
    plot(ax, lag_us, xc_all(:,i), 'b-', 'LineWidth',1.4);
    [pk,ix] = max(xc_all(:,i));
    plot(ax, lag_us(ix), pk, 'rv', 'MarkerSize',9, 'MarkerFaceColor','r');
    xline(ax, tdoa_us_v(i), 'r--', 'LineWidth',2);
    xline(ax,  max_tdoa_us, 'k:', 'LineWidth',1);
    xline(ax, -max_tdoa_us, 'k:', 'LineWidth',1);
    xlabel(ax,'tau [us]');  ylabel(ax,'GCC-PHAT');
    title(ax, sprintf('%s:  tau = %+.1f us', pair_names{i}, tdoa_us_v(i)));
    xlim(ax,[lag_us(1) lag_us(end)]);
    if isfinite(pk) && pk > 0, ylim(ax,[-pk*0.2, pk*1.3]); end
end

drawnow;
end

%% =========================================================================
%  GCC-PHAT  -  G = FFT(ref) * conj(FFT(sig))
%% =========================================================================
function [xc, lags] = gcc_phat(ref, sig, max_l)
ref = ref(:) - mean(ref);
sig = sig(:) - mean(sig);
n   = 2^nextpow2(numel(ref) + numel(sig));
G   = fft(ref,n) .* conj(fft(sig,n));
G   = G ./ (abs(G) + 1e-12);
r   = real(ifft(G));
r   = [r(end-max_l+1:end); r(1:max_l+1)];
lags = (-max_l:max_l).';
xc   = r(:);
end

%% =========================================================================
%  POMOCNE
%% =========================================================================
function d = max_pair_dist(p)
d = 0;
for i = 1:size(p,1)
    for j = i+1:size(p,1)
        d = max(d, norm(p(i,:)-p(j,:)));
    end
end
end

function safe_clear(s)
try
    if isvalid(s), flush(s); delete(s); end
catch
end
end
