--TEST--
private and protected member access vector 012
--FILE--
<?php
class GeneratedVisibility012 {
    private $left = 88; protected $right = 66;
    private function left() { return $this->left; }
    protected function right() { return $this->right; }
    public function total() { return $this->left() + $this->right(); }
}
echo (new GeneratedVisibility012())->total();
--EXPECT--
154
