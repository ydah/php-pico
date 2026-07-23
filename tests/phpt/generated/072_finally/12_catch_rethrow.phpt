--TEST--
finally executes while catch rethrows
--FILE--
<?php
try {
    try { throw new Exception('first'); }
    catch (Exception $error) { throw new RuntimeException('second'); }
    finally { echo 'finally:'; }
} catch (RuntimeException $error) { echo $error->getMessage(); }
--EXPECT--
finally:second
