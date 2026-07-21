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
class DynamicBox {
    public $items = [3, 1, 2];
    public function first() { return $this->items[0]; }
}
$box = new DynamicBox();
$property = 'items';
$method = 'first';
$copy = $box->items;
$box->{$property}[0] += 4;
sort($box->items);
echo ':', $copy[0], ':', implode(',', $box->items), ':', $box->$method();
--EXPECT--
1:1:3:5::3:7:0:3:1,2,7:1
