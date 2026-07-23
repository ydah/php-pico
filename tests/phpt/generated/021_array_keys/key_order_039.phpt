--TEST--
associative key order vector 039
--FILE--
<?php
$items = ['left' => 49, 4 => 137, 'right' => 225];
unset($items[4]);
$items['tail'] = 226;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:49,225,226:1
