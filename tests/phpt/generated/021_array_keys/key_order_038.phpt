--TEST--
associative key order vector 038
--FILE--
<?php
$items = ['left' => 48, 4 => 134, 'right' => 220];
unset($items[4]);
$items['tail'] = 221;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:48,220,221:1
