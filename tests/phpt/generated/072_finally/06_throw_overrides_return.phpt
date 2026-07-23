--TEST--
finally throw overrides a pending return
--FILE--
<?php
function final_throw_over_return() {
    try { return 4; } finally { throw new Exception('override'); }
}
try { final_throw_over_return(); }
catch (Exception $error) { echo $error->getMessage(); }
--EXPECT--
override
