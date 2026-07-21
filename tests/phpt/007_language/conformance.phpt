--TEST--
casts, nested lvalues, nullsafe access, interpolation, and conditional functions
--FILE--
<?php
declare(strict_types=1);
$source = array('node' => array('value' => 1));
$copy = $source;
$copy['node']['value'] += 2;
$copy['node']['next'] ??= 4;
$key = 'value';
$object = null;
if (true) {
    function selected_function() { return 7; }
}
if (false) {
    function skipped_function() { return 1; }
}
echo (int)'1.9', ':', $source['node']['value'], ':', $copy['node']['value'], ':';
echo ++$copy['node']['next'], ':', $object?->missing(), ':', "{$copy['node'][$key]}", ':';
echo selected_function(), ':', function_exists('skipped_function') ? 1 : 0;
--EXPECT--
1:1:3:5::3:7:0
