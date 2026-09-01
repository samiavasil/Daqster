#ifndef STREAMURLVALIDATOR_H
#define STREAMURLVALIDATOR_H

#include <QString>

/**
 * @brief Validates user-entered stream URL strings for the Stream Source node.
 *
 * Flat namespace style (like VideoCompat/VideoTransformOps): a single free
 * function with no node/widget state, so the validation logic can be shared
 * with the node UI and unit tested headlessly. QtCore-only (QString/QUrl).
 */
namespace StreamUrlValidator {

/**
 * @brief Validate a stream URL string for http/https/rtsp playback.
 *
 * The check mirrors the Stream Source node contract: a non-empty (after
 * trimming) input, a QUrl with a known scheme, and a scheme that the
 * node can play (http, https or rtsp, case-insensitive).
 *
 * @param urlString Raw URL text as entered by the user.
 * @param errorOut  Optional pointer that receives a user-facing error message
 *                  when the URL is rejected (left untouched on success).
 * @return True when the URL is acceptable, false otherwise.
 */
bool isValidStreamUrl(const QString &urlString, QString *errorOut = nullptr);

} // namespace StreamUrlValidator

#endif // STREAMURLVALIDATOR_H
