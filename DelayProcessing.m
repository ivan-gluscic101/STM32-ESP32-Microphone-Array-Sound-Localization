% =========================================================================
%  Kontinuirana TDOA lokalizacija izvora zvuka - STM32G474RE + MATLAB
%  Tok: [0xFF 0xFF 0xFF 0xFF] + 4-kanalni uint16 interleaved ADC blok
% =========================================================================

clear; clc; close all;

%% -------------------------------------------------------------------------
%  KONFIGURACIJA - MORA odgovarati app_config.h na STM32 strani
% -------------------------------------------------------------------------
COM_PORT       = "COM5";
BAUD_RATE      = 3000000;          % FT232RL maksimum je 3 Mbaud
NUM_CHANNELS   = 4;
SAMPLES_PER_CH = 1024;
NUM_FRAMES_BUF = 4;                % 4*1024 uzoraka/kanalu za obradu ~=64 ms
FS             = 170e6 / 5312;     % stvarni TIM3 Fs za ARR=5311: 32003.012 Hz

C_SOUND        = 343.0;            % m/s, prilagodi za temperaturu po potrebi
HPF_CUTOFF_HZ  = 300;
LPF_CUTOFF_HZ  = 10000;            % mora biti < FS/2; za 32 kHz je 10 kHz OK

% Redoslijed MORA odgovarati ADC rankovima:
% MIC1 = PB14/ADC1_IN5, MIC2 = PC0/ADC1_IN6, MIC3 = PC1/ADC1_IN7, MIC4 = PC2/ADC1_IN8
MIC_POS = [
    0,      0,      0;             % MIC1
    0.0867, 0.05,   0;             % MIC2
    0.0867,-0.05,   0;             % MIC3
    0.05,   0,      0.08           % MIC4
];

% Kalibracijske korekcije po paru MIC1-MICi [us].
% Ako svi kanali imaju jednaku elektroniku, ostavi 0. Za preciznost izmjeri
% zvuk iz poznatog smjera i ovdje upiši sustavni offset.
TDOA_OFFSET_US = [0 0 0];

CLAP_THRESHOLD_DB = 14;             % prag iznad adaptivnog noise floora
PRE_EVENT_MS      = 8;
POST_EVENT_MS     = 22;
REFRACTORY_SEC    = 0.18;           % ne prijavljuj isti impuls više puta
PLOT_EVERY_N_FRAMES = 2;

SYNC = uint8([255; 255; 255; 255]);
PAYLOAD_BYTES = NUM_CHANNELS * SAMPLES_PER_CH * 2;
PACKET_BYTES  = numel(SYNC) + PAYLOAD_BYTES;
BLOCK_SAMPLES = SAMPLES_PER_CH * NUM_FRAMES_BUF;

max_mic_distance = max_pair_distance(MIC_POS);
MAX_TDOA_US = max_mic_distance / C_SOUND * 1e6;
MAX_LAG     = ceil(MAX_TDOA_US * 1e-6 * FS * 1.25);

fprintf("UART brzina: %.1f Mbaud  (FT232RL limit)\n", BAUD_RATE/1e6);
fprintf("Ocekivani tok: %.1f kB/s payload, %.2f Mbit/s na UART 8N1\n", ...
    NUM_CHANNELS*FS*2/1000, NUM_CHANNELS*FS*2*10/1e6);
fprintf("Frame: %d B svakih %.2f ms\n", PACKET_BYTES, SAMPLES_PER_CH/FS*1e3);

%% -------------------------------------------------------------------------
%  SERIJSKI PORT I LIVE BUFFER
% -------------------------------------------------------------------------
s = serialport(COM_PORT, BAUD_RATE, "Timeout", 0.20);
s.ByteOrder = "little-endian";
flush(s);

rxbuf = uint8([]);
sigbuf = zeros(BLOCK_SAMPLES, NUM_CHANNELS);
frames_rx = 0;
packets_lost_sync = 0;
last_event_tic = -Inf;
last_result = struct("az", NaN, "el", NaN, "u", [NaN;NaN;NaN], ...
                     "tdoa_us", [NaN NaN NaN], "quality", 0);

[b_bp, a_bp] = butter(3, [HPF_CUTOFF_HZ LPF_CUTOFF_HZ]/(FS/2), "bandpass");

fig = figure("Name", "Kontinuirana TDOA lokalizacija", "Color", "w", ...
             "Position", [40 60 1180 760]);
tiledlayout(fig, 3, 2, "TileSpacing", "compact", "Padding", "compact");
axRaw = nexttile(1, [1 2]); grid(axRaw,"on"); hold(axRaw,"on");
axE   = nexttile(3); grid(axE,"on"); hold(axE,"on");
axT   = nexttile(4); grid(axT,"on"); hold(axT,"on");
axD   = nexttile(5, [1 2]); grid(axD,"on"); hold(axD,"on"); axis(axD,"equal");
view(axD, 35, 22);

fprintf("\nCekam kontinuirani stream. Za prekid zatvori figure ili pritisni Ctrl+C.\n");

%% -------------------------------------------------------------------------
%  GLAVNA PETLJA
% -------------------------------------------------------------------------
while isvalid(fig)
    n = s.NumBytesAvailable;
    if n > 0
        newBytes = read(s, n, "uint8");
        rxbuf = [rxbuf; uint8(newBytes(:))]; %#ok<AGROW>
    else
        pause(0.002);
    end

    [frames, rxbuf, lost_sync_now] = parse_packets(rxbuf, SYNC, PAYLOAD_BYTES, ...
                                                   NUM_CHANNELS, SAMPLES_PER_CH);
    packets_lost_sync = packets_lost_sync + lost_sync_now;

    for k = 1:numel(frames)
        frame = frames{k};
        frames_rx = frames_rx + 1;
        sigbuf = [sigbuf(SAMPLES_PER_CH+1:end, :); frame]; %#ok<AGROW>

        if frames_rx < NUM_FRAMES_BUF
            continue;
        end

        [result, event_detected, proc] = process_tdoa_block(sigbuf, FS, C_SOUND, ...
            MIC_POS, MAX_LAG, TDOA_OFFSET_US, b_bp, a_bp, CLAP_THRESHOLD_DB, ...
            PRE_EVENT_MS, POST_EVENT_MS);

        tnow = toc_or_zero();
        if event_detected && (tnow - last_event_tic) > REFRACTORY_SEC
            last_event_tic = tnow;
            last_result = result;
            fprintf("[%7.2fs] Azimut=%+7.2f deg | Elevacija=%+7.2f deg | TDOA=[%+7.2f %+7.2f %+7.2f] us | Q=%.2f\n", ...
                tnow, result.az, result.el, result.tdoa_us(1), result.tdoa_us(2), ...
                result.tdoa_us(3), result.quality);
        end

        if mod(frames_rx, PLOT_EVERY_N_FRAMES) == 0
            update_plots(axRaw, axE, axT, axD, sigbuf, proc, last_result, frames_rx, ...
                         packets_lost_sync, FS, MIC_POS, MAX_TDOA_US);
            drawnow limitrate;
        end
    end
end

clear s;

%% =========================================================================
%  LOKALNE FUNKCIJE
% =========================================================================
function [frames, rxbuf, lost_sync] = parse_packets(rxbuf, sync, payload_bytes, n_ch, n_samp)
    frames = {};
    lost_sync = 0;
    packet_bytes = numel(sync) + payload_bytes;

    while numel(rxbuf) >= numel(sync)
        idx = find_sync(rxbuf, sync);
        if isempty(idx)
            % Zadrzi zadnja 3 bajta za slučaj da sync počinje na granici čitanja.
            keep = min(numel(rxbuf), numel(sync)-1);
            lost_sync = lost_sync + max(0, numel(rxbuf) - keep);
            rxbuf = rxbuf(end-keep+1:end);
            return;
        end

        if idx > 1
            lost_sync = lost_sync + idx - 1;
            rxbuf = rxbuf(idx:end);
        end

        if numel(rxbuf) < packet_bytes
            return;
        end

        payload = rxbuf(numel(sync)+1 : packet_bytes);
        rxbuf = rxbuf(packet_bytes+1:end);

        u16 = typecast(uint8(payload), "uint16");
        mat = reshape(double(u16), n_ch, n_samp).';
        frames{end+1} = mat; %#ok<AGROW>
    end
end

function idx = find_sync(buf, sync)
    idx = [];
    if numel(buf) < numel(sync)
        return;
    end
    hit = strfind(buf(:).', sync(:).');
    if ~isempty(hit)
        idx = hit(1);
    end
end

function [result, event_detected, proc] = process_tdoa_block(x_raw, fs, c, mic_pos, max_lag, tdoa_offset_us, b, a, thr_add_db, pre_ms, post_ms)
    x = double(x_raw);
    x = x - median(x, 1);
    x_f = filtfilt(b, a, x);

    energy = mean(x_f.^2, 2);
    env_win = max(8, round(0.8e-3 * fs));
    env = movmean(energy, env_win);
    env_db = 10*log10(env + eps);
    nf_db = prctile(env_db, 20);
    thr_db = nf_db + thr_add_db;
    [pk_db, peak_sample] = max(env_db);
    event_detected = pk_db > thr_db;

    win_start = max(1, peak_sample - round(pre_ms*1e-3*fs));
    win_end   = min(size(x_f,1), peak_sample + round(post_ms*1e-3*fs));
    if win_end - win_start + 1 < 128
        win_start = max(1, peak_sample - 64);
        win_end = min(size(x_f,1), peak_sample + 64);
    end
    xw = x_f(win_start:win_end, :);

    tdoa_samples = zeros(1,3);
    tdoa_us = zeros(1,3);
    peak_val = zeros(1,3);
    for i = 2:4
        [xc, lags] = gcc_phat_delay(xw(:,1), xw(:,i), max_lag);
        [pk, ix] = max(xc);
        lag = refine_peak(lags, xc, ix);
        tdoa_samples(i-1) = lag;
        tdoa_us(i-1) = lag / fs * 1e6 - tdoa_offset_us(i-1);
        peak_val(i-1) = pk;
    end

    A = mic_pos(1,:) - mic_pos(2:end,:);
    tau_s = tdoa_us(:) * 1e-6;
    u = A \ (c * tau_s);

    % Ako šum da normu > 1, projekcija na jedinicnu kuglu daje najbliži DOA.
    nu = norm(u);
    if nu < 1e-9
        u_unit = [NaN; NaN; NaN];
        az = NaN; el = NaN;
    else
        u_unit = u / max(1, nu);
        u_unit = u_unit / norm(u_unit);
        az = atan2d(u_unit(2), u_unit(1));
        el = asind(max(-1, min(1, u_unit(3))));
    end

    physically_ok = all(abs(tdoa_us) <= 1.10 * max_pair_distance(mic_pos)/c*1e6);
    quality = mean(peak_val) * double(physically_ok) * double(event_detected);

    result = struct("az", az, "el", el, "u", u_unit, "tdoa_us", tdoa_us, ...
                    "quality", quality, "tdoa_samples", tdoa_samples, ...
                    "peak", peak_val);
    proc = struct("x_f", x_f, "env_db", env_db, "nf_db", nf_db, "thr_db", thr_db, ...
                  "peak_sample", peak_sample, "win_start", win_start, "win_end", win_end, ...
                  "event", event_detected);
end

function [xc, lags] = gcc_phat_delay(ref, sig, max_lag)
    % Pozitivan lag znaci: sig kasni za ref, tj. t_sig - t_ref > 0.
    ref = ref(:) - mean(ref);
    sig = sig(:) - mean(sig);
    n = 2^nextpow2(numel(ref) + numel(sig));
    REF = fft(ref, n);
    SIG = fft(sig, n);
    G = SIG .* conj(REF);
    G = G ./ (abs(G) + 1e-12);
    r = real(ifft(G));
    r = [r(end-max_lag+1:end); r(1:max_lag+1)];
    lags = (-max_lag:max_lag).';
    xc = r(:);
end

function lag = refine_peak(lags, xc, ix)
    lag = double(lags(ix));
    if ix <= 1 || ix >= numel(xc)
        return;
    end
    y1 = xc(ix-1); y2 = xc(ix); y3 = xc(ix+1);
    den = y1 - 2*y2 + y3;
    if abs(den) > 1e-12
        frac = 0.5 * (y1 - y3) / den;
        if abs(frac) <= 1
            lag = lag + frac;
        end
    end
end

function update_plots(axRaw, axE, axT, axD, sigbuf, proc, result, frames_rx, lost_sync, fs, mic_pos, max_tdoa_us)
    t_ms = (0:size(sigbuf,1)-1)/fs*1e3;

    cla(axRaw);
    plot(axRaw, t_ms, sigbuf - median(sigbuf,1));
    title(axRaw, sprintf("Kontinuirani ADC tok | frame=%d | izgubljeni sync bajtovi=%d", frames_rx, lost_sync));
    xlabel(axRaw, "Vrijeme [ms]"); ylabel(axRaw, "ADC - median");
    legend(axRaw, "MIC1", "MIC2", "MIC3", "MIC4", "Location", "northeast");
    grid(axRaw,"on");

    cla(axE);
    tt = (0:numel(proc.env_db)-1)/fs*1e3;
    plot(axE, tt, proc.env_db, "LineWidth", 1.0); hold(axE,"on");
    yline(axE, proc.nf_db, "--"); yline(axE, proc.thr_db, "-");
    xline(axE, proc.peak_sample/fs*1e3, "--");
    xline(axE, proc.win_start/fs*1e3, ":"); xline(axE, proc.win_end/fs*1e3, ":");
    title(axE, "Energija i detekcija dogadaja");
    xlabel(axE, "Vrijeme [ms]"); ylabel(axE, "dB"); grid(axE,"on");

    cla(axT);
    bar(axT, 1:3, result.tdoa_us);
    yline(axT, max_tdoa_us, "r:"); yline(axT, -max_tdoa_us, "r:"); yline(axT, 0, "k--");
    xticks(axT, 1:3); xticklabels(axT, {"M1-M2", "M1-M3", "M1-M4"});
    ylabel(axT, "TDOA [us]");
    title(axT, sprintf("Az=%+.1f deg, El=%+.1f deg, Q=%.2f", result.az, result.el, result.quality));
    grid(axT,"on");

    cla(axD); hold(axD,"on"); grid(axD,"on"); axis(axD,"equal");
    [xs,ys,zs] = sphere(24);
    surf(axD, xs, ys, zs, "FaceAlpha", 0.06, "EdgeAlpha", 0.05);
    quiver3(axD,0,0,0,1.15,0,0,0,"k"); text(axD,1.25,0,0,"X");
    quiver3(axD,0,0,0,0,1.15,0,0,"k"); text(axD,0,1.25,0,"Y");
    quiver3(axD,0,0,0,0,0,1.15,0,"k"); text(axD,0,0,1.25,"Z");

    center = mean(mic_pos,1);
    scale = 1.3 / max_pair_distance(mic_pos);
    mp = (mic_pos - center) * scale;
    plot3(axD, mp(:,1), mp(:,2), mp(:,3), "ks", "MarkerFaceColor", "y", "MarkerSize", 7);
    for i = 1:4
        text(axD, mp(i,1), mp(i,2), mp(i,3), sprintf(" M%d", i));
    end

    if all(isfinite(result.u))
        quiver3(axD, 0,0,0, result.u(1), result.u(2), result.u(3), 0, "r", "LineWidth", 3, "MaxHeadSize", 0.35);
        plot3(axD, [0 result.u(1)], [0 result.u(2)], [0 0], "r:");
    end
    xlabel(axD,"X"); ylabel(axD,"Y"); zlabel(axD,"Z");
    title(axD, "Smjer dolaska zvuka");
    xlim(axD,[-1.2 1.2]); ylim(axD,[-1.2 1.2]); zlim(axD,[-1.2 1.2]);
    view(axD,35,22);
end

function dmax = max_pair_distance(p)
    dmax = 0;
    for i = 1:size(p,1)
        for j = i+1:size(p,1)
            dmax = max(dmax, norm(p(i,:) - p(j,:)));
        end
    end
end

function t = toc_or_zero()
    persistent t0
    if isempty(t0)
        t0 = tic;
        t = 0;
    else
        t = toc(t0);
    end
end
