#pragma once

// Resultado de un intento de sincronización
enum class SyncResult {
    ACCESSED,  // pudo obtener el recurso
    WAITING    // se bloqueó
};

// Tipo de operación de sincronización
enum class SyncAction {
    READ,
    WRITE,
    ADQUIRE,
    RELEASE,
    WAIT,
    SIGNAL,
    WAKE
};