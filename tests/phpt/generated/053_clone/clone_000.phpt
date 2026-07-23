--TEST--
object clone and property COW vector 000
--FILE--
<?php
class GeneratedClone000 { public $items; public function __construct($items) { $this->items = $items; } }
$original = new GeneratedClone000([3, 5]);
$copy = clone $original;
$copy->items[0] = 8;
echo implode(',', $original->items), ':', implode(',', $copy->items);
--EXPECT--
3,5:8,5
