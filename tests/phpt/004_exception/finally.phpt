--TEST--
finally runs while returning
--FILE--
<?php
function answer() {
    try { return 40; }
    finally { echo 'before:'; }
}
echo answer() + 2;
--EXPECT--
before:42
