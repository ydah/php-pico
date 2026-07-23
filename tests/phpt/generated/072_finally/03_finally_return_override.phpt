--TEST--
finally return overrides try return
--FILE--
<?php
function final_return_override() {
    try { return 3; } finally { return 9; }
}
echo final_return_override();
--EXPECT--
9
