# REQ-SW-PL-033: Qt6 Camera Audio Capture (FFmpeg)

- **Статус:** FUTURE (backlog — не се имплементира сега)
- **Приоритет:** P2 (backlog)
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-27
- **Родител:** —
- **Зависи от:** REQ-SW-PL-022 (sample порт)

## Статус: FUTURE (backlog — не се имплементира сега)

## Описание
На Qt6 няма публичен Qt Multimedia API за получаване на жив аудио поток от камера.
CameraSourceNode sample портът (port 1) емитива invalid data на Qt6.
Изискването е да се добави платформен аудио capture път (FFmpeg аудио устройство capture),
който да доставя живи аудио буфери от вградения микрофон на камерата.

## Мотивация
- REQ-SW-PL-022 дефинира sample порт (SampledData) за всички source нодове
- VideoFileSourceNode и StreamSourceNode имат аудио на Qt6 (QMediaPlayer/QMediaCaptureSession декодират аудио)
- CameraSourceNode е единственият без аудио на Qt6 — Qt платформена лимитация

## Технически анализ (Qt 6.x)
- QAudioBufferOutput: "used for capturing audio data provided by QMediaPlayer" — само playback
- QAudioBufferInput: инжектира custom аудио в QMediaRecorder — не чете от камера
- QMediaCaptureSession: няма setAudioBufferOutput() — не излага буфери
- Qt5 QAudioProbe (работеше с capture session) е премахнат в Qt6
- Решение: FFmpeg аудио устройство capture + device matching (кой микрофон е на камерата)

## Acceptance Criteria
- [ ] AC 1: CameraSourceNode sample портът (port 1) емитира живи аудио буфери на Qt6
- [ ] AC 2: Аудио буферите са SampledData (domain "audio") — същия формат като VideoFileSourceNode
- [ ] AC 3: Device matching: вграденият микрофон на камерата се открива автоматично
- [ ] AC 4: Работи на Linux и Windows
- [ ] AC 5: Qt5 пътят (QAudioProbe) остава непроменен
- [ ] AC 6: Builds Qt5/Qt6 PASS + ctest PASS

## Зависимости
- REQ-SW-PL-022 (sample порт)

## Бележки
- Не се имплементира в текущия спринт — бъдеща задача