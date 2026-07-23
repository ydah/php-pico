--TEST--
outer finally runs after inner throw is caught
--FILE--
<?php
$log = [];
try {
    try { throw new Exception('x'); }
    catch (Exception $error) { $log[] = 'inner-catch'; }
    $log[] = 'after-catch';
} finally { $log[] = 'outer-finally'; }
echo implode(',', $log);
--EXPECT--
inner-catch,after-catch,outer-finally
