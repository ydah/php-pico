--TEST--
object clone and property COW vector 001
--FILE--
<?php
class GeneratedClone001 { public $items; public function __construct($items) { $this->items = $items; } }
$original = new GeneratedClone001([4, 7]);
$copy = clone $original;
$copy->items[0] = 17;
echo implode(',', $original->items), ':', implode(',', $copy->items);
--EXPECT--
4,7:17,7
