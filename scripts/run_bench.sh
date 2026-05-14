echo "QNX IPC Benchmark — Payload Sweep "
echo ""

echo "[1/2] Message Passing benchmark"
echo "      Starting server..."
/tmp/msg_server &
SERVER_PID=$!
sleep 1

echo "      Starting client..."
/tmp/msg_client
wait $SERVER_PID

echo ""
echo "[2/2] Shared Memory benchmark"
echo "      Starting writer..."
/tmp/shm_writer &
sleep 0.5

echo "      Starting reader..."
/tmp/shm_reader

echo ""
echo "All done. CSVs saved to /tmp/ "
echo "      Copy to /fs/ with:"
echo "      cp /tmp/*summary*.csv /tmp/*results_*B.csv /fs/"