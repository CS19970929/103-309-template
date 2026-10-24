#ifndef SCI_PROTOCOL_H
#define SCI_PROTOCOL_H

/* Internal protocol boundary. A port lends its buffer to one complete request
 * at a time; validation has no writes or protection side effects. */
void Sci_ResetMessage(struct RS485MSG *message);
UINT8 Sci_ModbusFeed(struct RS485MSG *message, UINT8 data);
UINT8 Sci_ModbusFrameValid(const struct RS485MSG *message);
void Sci_ModbusProcessFrame(struct RS485MSG *message);
UINT8 Sci_CustomerFeed(struct RS485MSG *message, UINT8 data);
UINT8 Sci_CustomerFrameValid(const struct RS485MSG *message);
void Sci_CustomerProcessFrame(struct RS485MSG *message);
void Sci_CustomerUpdateCounters(void);

#endif
