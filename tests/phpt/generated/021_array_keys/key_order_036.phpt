--TEST--
associative key order vector 036
--FILE--
<?php
$items = ['left' => 46, 4 => 128, 'right' => 210];
unset($items[4]);
$items['tail'] = 211;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:46,210,211:1
