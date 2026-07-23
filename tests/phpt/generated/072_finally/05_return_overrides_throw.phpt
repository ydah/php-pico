--TEST--
finally return overrides a pending throw
--FILE--
<?php
function final_return_over_throw() {
    try { throw new Exception('discarded'); } finally { return 12; }
}
echo final_return_over_throw();
--EXPECT--
12
