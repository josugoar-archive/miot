import enum


class OracleEvents(enum.StrEnum):
    SENT_REQUEST = "SentRequest"
    CANCELLED_REQUEST = "CancelledRequest"
