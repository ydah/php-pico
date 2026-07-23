--TEST--
finally runs before a throw propagates
--FILE--
<?php
try {
    try { throw new Exception('boom'); } finally { echo 'finally:'; }
} catch (Exception $error) { echo $error->getMessage(); }
--EXPECT--
finally:boom
